/*
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Config.hpp"
#include "StoreLogger.hpp"
#include "ParameterFramework.hpp"

#include <catch.hpp>

#include <list>
#include <string>

#include <cstdio>

#ifndef SCENARIO_METHOD
    /** SCENARIO_METHOD is not available in catch on ubuntu 12.04 */
#   define SCENARIO_METHOD(className, ...) \
        TEST_CASE_METHOD(className, "Scenario: " __VA_ARGS__)
#endif

namespace parameterFramework
{

template <class Value>
struct Test
{
    std::string title;
    Value payload;
};

/** Using C style array instead of a C++ collection to workaround
 *  initializer list not supporting move semantic.
 */
template <class Value>
using Tests = Test<Value>[];

/** Defer Parameter Framework creation.
 * A custom configuration can be provided.
 */
class LazyPF
{
public:
    using PF = ParameterFramework;

    void create(Config &&configFile)
    {
        mPf.reset(new PF{ std::move(configFile) });
    }
    std::unique_ptr<PF> mPf;
};

/** PF that will log a warning at start. */
struct WarningPF : public ParameterFramework
{
    WarningPF() :
        ParameterFramework{ { &Config::domains, "<InvalidDomain/>" } }
    {
        setFailureOnFailedSettingsLoad(false);
    }
};

SCENARIO_METHOD(ParameterFramework, "Default logger", "[log]") {
    WHEN("No logger is set") {
        THEN("Start should succeed") {
            CHECK_NOTHROW(start());
        }
    }
}

SCENARIO_METHOD(ParameterFramework, "No Logger", "[log]") {
    WHEN("A nullptr logger is set") {
        setLogger(nullptr);
        THEN("Start should succeed") {
            CHECK_NOTHROW(start());
        }
    }
}

SCENARIO_METHOD(WarningPF, "Logger should receive info and warnings", "[log]") {
    GIVEN("Config files that emit warnings") {
        GIVEN("A logger that stores logs") {
            StoreLogger logger{};
            WHEN("The record logger is set") {
                setLogger(&logger);
                THEN("Start should succeed") {
                    REQUIRE_NOTHROW(start());
                    AND_THEN("The logger should have stored info and warning log") {
                        using Logs = StoreLogger::Logs;
                        using Level = StoreLogger::Log::Level;
                        CHECK(logger.filter(Level::warning) != Logs{});
                        CHECK(logger.getLogs() != Logs{});
                    }
                }
                AND_WHEN("A nullptr logger is set") {
                    setLogger(nullptr);
                    THEN("Start should succeed") {
                        REQUIRE_NOTHROW(start());
                        AND_THEN("The record logger should NOT have stored any info or warning log") {
                            CHECK(logger.getLogs() == StoreLogger::Logs{});
                        }
                    }
                }
            }
        }
    }
}

SCENARIO_METHOD(LazyPF, "Tuning OK", "[properties][remote interface]") {
}

SCENARIO_METHOD(LazyPF, "Invalid XML configuration") {
    for (auto &xmlT : Tests<std::string>{
            {"an unknown tag", "<unknown_tag/>" },
            {"an unclosed tag", "<unclosed>" } }) {
        auto invalidXml = xmlT.payload;
        GIVEN("An invalid xml: containing " + xmlT.title) {
            Config::Plugins ps{};
            for (auto &&configT : Tests<Config>{
                    {"top config", { &Config::plugins, { { "", { invalidXml } } } } },
                    {"structure", { &Config::instances, invalidXml } },
                    {"settings", { &Config::domains, invalidXml } } }) {
                WHEN("Used in the " + configT.title) {
                    create(std::move(configT.payload));
                    THEN("Start should fail") {
                        CHECK_THROWS_AS(mPf->start(), Exception);
                    }
                }
            }
        }
    }
}

SCENARIO_METHOD(LazyPF, "Floating points", "[floating point]") {
    auto validInstances = Config{ &Config::instances,
        // Size is fixed at 32 and as such is optional */
        R"(<FloatingPointParameter Name="Empty"/>
        <FloatingPointParameter Name="trivial" Size="32"/>
        <FloatingPointParameter Name="nominal" Size="32" Min="-50.4" Max="12.2"/>
        <FloatingPointParameter Name="defaultMin" Size="32" Max="12.2"/>
        <FloatingPointParameter Name="defaultMax" Size="32" Min="-50.4"/>)"
    };
    const auto &invalidParameters = Tests<std::string>{
        { "invalid Size(64)", "<FloatingPointParameter Name='error' Size='64'/>" },
        { "invalid Size(16)", "<FloatingPointParameter Name='error' Size='16'/>" },
        { "minimum > maximum", "<FloatingPointParameter Name='error' Min='1' Max='0'/>" }
    };

    GIVEN("A Structure using an invalid FloatingPointParameters") {
        for (auto &vec : invalidParameters) {
            GIVEN("intentional error: " + vec.title) {
                create(Config{ &Config::instances, vec.payload });
                THEN("Start should fail") {
                    CHECK_THROWS_AS(mPf->start(), Exception);
                }
            }
        }
    }

    GIVEN("A Structure using valid FloatingPointParameters") {
        create(std::move(validInstances));

        THEN("Start should succeed") {
            CHECK_NOTHROW(mPf->start());
            REQUIRE_NOTHROW(mPf->setTuningMode(true));
            std::string path = "/test/test/nominal";

            AND_THEN("Set/Get a floating point parameter in real value space") {

                for (auto &vec : Tests<std::string>{
                            { "(too high)", "12.3" },
                            { "(too low)", "-50.5" },
                            { "(not a number)", "foobar" },
                        }) {
                    GIVEN("Invalid value " + vec.title) {
                        CHECK_THROWS_AS(mPf->setParameter(path, vec.payload), Exception);
                    }
                }
                for (auto &vec : Tests<std::string>{
                            { "(upper limit)", "12.2" },
                            { "(lower limit)", "-50.4" },
                            { "(inside range)", "0" },
                        }) {
                    GIVEN("A valid value " + vec.title) {
                        CHECK_NOTHROW(mPf->setParameter(path, vec.payload));
                        std::string getValueBack;
                        CHECK_NOTHROW(mPf->getParameter(path, getValueBack));
                        CHECK(getValueBack == vec.payload);
                    }
                }
            }

            AND_THEN("Set/Get a floating point parameter in raw value space") {
                const float tooHigh = 12.3f;
                const float tooLow = -50.5f;
                REQUIRE_NOTHROW(mPf->setRawValueSpace(true));
                for (auto &vec : Tests<std::string>{
                            { "(too high, as decimal)",
                                std::to_string(reinterpret_cast<const uint32_t&>(tooHigh)) },
                            { "(too low, as decimal)",
                                std::to_string(reinterpret_cast<const uint32_t&>(tooLow)) },
                            { "(meaningless)", "foobar" },
                            { "(infinity)", std::to_string(std::numeric_limits<float>::infinity())},
                            { "(NaN)", std::to_string(std::numeric_limits<float>::quiet_NaN())},
                        }) {
                    GIVEN("Invalid value " + vec.title) {
                        CHECK_THROWS_AS(mPf->setParameter(path, vec.payload), Exception);
                    }
                }
                const float upper = 12.2f;
                const float lower = -50.4f;
                const float zero = 0.0f;
                for (auto &vec : Tests<std::string>{
                            { "(upper limit, as decimal)",
                                std::to_string(reinterpret_cast<const uint32_t&>(upper)) },
                            { "(lower limit, as decimal)",
                                std::to_string(reinterpret_cast<const uint32_t&>(lower)) },
                            { "(inside range, as decimal)",
                                std::to_string(reinterpret_cast<const uint32_t&>(zero)) },
                        }) {
                    GIVEN("A valid value " + vec.title) {
                        CHECK_NOTHROW(mPf->setParameter(path, vec.payload));
                        std::string getValueBack;
                        CHECK_NOTHROW(mPf->getParameter(path, getValueBack));
                        CHECK(getValueBack == vec.payload);
                    }
                }
            }

            AND_THEN("Set/Get floating point parameter handle") {
                /** @TODO: use move semantics to get an owned object so that
                 * it will destroyed automatically */
                ParameterHandle *handle;
                CHECK_NOTHROW(handle = mPf->createParameterHandle(path));
                /** @FIXME: 'set' operations on a ParameterHandle are silently
                 * ignored in tuning mode. Does it make sense ? */
                REQUIRE_NOTHROW(mPf->setTuningMode(false));

                /* warning: even though the API below takes a double as
                 * argument, we need to define the test vector as floats in
                 * order to prevent rounding issues */
                for (auto &vec : Tests<float>{
                            { "(upper limit)", 12.2f },
                            { "(lower limit)", -50.4f },
                            { "(inside range)", 0.0f },
                        }) {
                    GIVEN("A valid value " + vec.title) {
                        CHECK_NOTHROW(handle->setAsDouble(vec.payload));
                        double getValueBack;
                        CHECK_NOTHROW(handle->getAsDouble(getValueBack));
                        CHECK(getValueBack == vec.payload);
                    }
                }
                for (auto &vec : Tests<float>{
                            { "(too high)", 12.3f },
                            { "(too low)", -50.5f },
                        }) {
                    GIVEN("An invalid value " + vec.title) {
                        CHECK_THROWS_AS(handle->setAsDouble(vec.payload), Exception);
                    }
                }
                delete handle;
            }
        }
    }
}

SCENARIO_METHOD(LazyPF, "Plugin OK", "[properties][missing plugin policy]") {
    for (auto &pluginNameT : Tests<std::string>{
            {"an non existing plugin", "libdonetexist.so" },
            {"an existing library but invalid (linux) PF plugin", "libc.so.6" } })
    {
        GIVEN("An" + pluginNameT.title)
        {
            create({ &Config::plugins, { { "", { pluginNameT.payload } } } });
            WHEN("The missing subsystem policy is left to default") {
                THEN("Start should fail") {
                    CHECK_THROWS_AS(mPf->start(), Exception);
                }
            }
            WHEN("The missing subsystem policy is set to failure") {
                mPf->setFailureOnMissingSubsystem(true);
                THEN("Start should fail") {
                    CHECK_THROWS_AS(mPf->start(), Exception);
                }
            }
            WHEN("The missing subsystem policy is set to ignore") {
                mPf->setFailureOnMissingSubsystem(false);
                THEN("Start should success") {
                    CHECK_NOTHROW(mPf->start());
                }
            }
        }
    }
}


SCENARIO_METHOD(LazyPF, "Invalid domains", "[properties]") {
    GIVEN("An invalid domain file") {
        create({ &Config::domains, "<Domain name='Invalid'/>" });
        THEN("Start should fail") {
            CHECK_THROWS_AS(mPf->start(), Exception);
        }
        WHEN("Changing failure setting load policy to ignore") {
            mPf->setFailureOnFailedSettingsLoad(false);
            THEN("Start should succeed") {
                CHECK_NOTHROW(mPf->start());
            }
        }
    }
}

SCENARIO_METHOD(ParameterFramework, "Raw value space") {
    WHEN("Raw value space is set") {
        setRawValueSpace(true);
        THEN("Value space should be raw") {
            CHECK(isValueSpaceRaw() == true);
        }
    }
}

} // parameterFramework

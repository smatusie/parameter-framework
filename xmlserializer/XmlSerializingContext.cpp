/* 
 * Copyright (c) 2011-2015, Intel Corporation
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
#include "XmlSerializingContext.h"
#include <libxml/xmlerror.h>
#include <cstdio>

CXmlSerializingContext::CXmlSerializingContext(std::string& strError) : _strError(strError)
{
    xmlSetStructuredErrorFunc(this, structuredErrorHandler);
}

CXmlSerializingContext::~CXmlSerializingContext()
{
    xmlSetGenericErrorFunc(NULL, NULL);
    _strError = _strXmlError + _strError;
}

// Error
void CXmlSerializingContext::setError(const std::string& strError)
{
    _strError = strError;
}

void CXmlSerializingContext::appendLineToError(const std::string& strAppend)
{
    _strError += "\n" + strAppend;
}

/** XML error handler
  *
  * @param[in] userData pointer to the serializing context
  * @param[in] format is the xml error output format
  *
  */
void CXmlSerializingContext::structuredErrorHandler(void* userData, xmlErrorPtr error)
{
    CXmlSerializingContext *self = static_cast<CXmlSerializingContext *>(userData);
    self->_strXmlError += error->message;
}

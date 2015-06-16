/*
Copyright (c) 2003-2004, Thees Christian Winkler
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the 
	  above copyright notice, this list of conditions and 
	  the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, 
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
    * Neither the name of the <ORGANIZATION> nor the names of its contributors
      may be used to endorse or promote products derived from this software without 
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
	SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
	TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
	EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "StdAfx.h"
#include "Tokenizer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTokenizer::CTokenizer(const CString& str, const CString& strDelim):
	m_str(str),
	m_nCurPos(0)
{
	SetDelimiters(strDelim);
}

void CTokenizer::SetDelimiters(const CString& strDelim)
{
	for(int i = 0; i < strDelim.GetLength(); ++i){

		m_bsDelim.set(static_cast<BYTE>(strDelim[i]));
	}
}

BOOL CTokenizer::Next(CString& str)
{
	str.Empty();

	while((m_nCurPos < m_str.GetLength()) && m_bsDelim[static_cast<BYTE>(m_str[m_nCurPos])]){

		++m_nCurPos;
	}

	if(m_nCurPos >= m_str.GetLength()){

		return FALSE;
	}

	int nStartPos = m_nCurPos;
	while((m_nCurPos < m_str.GetLength()) && !m_bsDelim[static_cast<BYTE>(m_str[m_nCurPos])]){

		++m_nCurPos;
	}
	
	str = m_str.Mid(nStartPos, m_nCurPos - nStartPos);

	return TRUE;
}

CString	CTokenizer::Tail() const
{
	int nCurPos = m_nCurPos;

	while((nCurPos < m_str.GetLength()) && m_bsDelim[static_cast<BYTE>(m_str[nCurPos])]){

		++nCurPos;
	}

	CString strResult;

	if(nCurPos < m_str.GetLength()){

		strResult = m_str.Mid(nCurPos);
	}

	return strResult;
}

/*
language.cpp

������ � lng �������
*/
/*
Copyright (c) 1996 Eugene Roshal
Copyright (c) 2000 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "headers.hpp"
#pragma hdrstop

#include "language.hpp"
#include "global.hpp"
#include "fn.hpp"
#include "lang.hpp"
#include "scantree.hpp"
#include "vmenu.hpp"
#include "manager.hpp"

extern wchar_t *ReadString (FILE *file, wchar_t *lpwszDest, int nDestLength, int nType);

#define LangFileMask L"*.lng"

#ifndef pack
 #define _PACK_BITS 2
 #define _PACK (1 << _PACK_BITS)
 #define pck(x,N)            ( ((x) + ((1<<(N))-1) )  & ~((1<<(N))-1) )
 #define pack(x)             pck(x,_PACK_BITS)
#endif

Language Lang;
static Language OldLang;

Language::Language()
{
  MsgList = NULL;
  MsgAddr = NULL;

  MsgListA = NULL;
  MsgAddrA = NULL;

  MsgCount=0;
  MsgSize=0;
}


int Language::Init(const wchar_t *Path,int CountNeed)
{
  if (MsgList!=NULL)
    return(TRUE);

  int LastError=GetLastError();

  int nCodePage = CP_OEMCP;
  string strLangName=Opt.strLanguage;
  FILE *LangFile=OpenLangFile(Path,LangFileMask,Opt.strLanguage,strMessageFile, nCodePage,FALSE, &strLangName);
  if(this == &Lang && StrCmpI(Opt.strLanguage,strLangName))
    Opt.strLanguage=strLangName;

  if (LangFile==NULL)
    return(FALSE);

  wchar_t ReadStr[1024];
  memset (&ReadStr, 0, sizeof (ReadStr));

  while ( ReadString (LangFile, ReadStr, sizeof (ReadStr)/sizeof (wchar_t), nCodePage) !=NULL )
  {
    string strDestStr;
    RemoveExternalSpaces(ReadStr);
    if ( *ReadStr != L'\"')
      continue;
    int SrcLength=(int)wcslen (ReadStr);

    if (ReadStr[SrcLength-1]==L'\"')
      ReadStr[SrcLength-1]=0;

    ConvertString(ReadStr+1,strDestStr);

    int DestLength=(int)pack(strDestStr.GetLength()+1);

    if ( (MsgList = (wchar_t*)xf_realloc(MsgList, (MsgSize+DestLength)*sizeof (wchar_t)))==NULL )
    {
      fclose(LangFile);
      return(FALSE);
    }

    if ( (MsgListA = (char*)xf_realloc(MsgListA, (MsgSize+DestLength)*sizeof (char))) == NULL )
    {
      xf_free (MsgList);
      fclose(LangFile);
      return FALSE;
    }

    *(int*)&MsgList[MsgSize+DestLength-_PACK] = 0;
    *(int*)&MsgListA[MsgSize+DestLength-_PACK] = 0;

    wcscpy(MsgList+MsgSize, strDestStr);

    UnicodeToAnsi (strDestStr, MsgListA+MsgSize, DestLength);

    MsgSize+=DestLength;
    MsgCount++;
  }
  //   �������� �������� �� ���������� ����� � LNG-������
  if(CountNeed != -1 && CountNeed != MsgCount-1)
  {
    fclose(LangFile);
    return(FALSE);
  }

  wchar_t *CurAddr = MsgList;
  char *CurAddrA = MsgListA;

  MsgAddr = new wchar_t*[MsgCount];

  if ( MsgAddr == NULL )
  {
    fclose(LangFile);
    return(FALSE);
  }

  MsgAddrA = new char*[MsgCount];

  if ( MsgAddrA == NULL )
  {
    delete[] MsgAddr;
    MsgAddr=NULL;
    fclose(LangFile);
    return FALSE;
  }

  for (int I=0;I<MsgCount;I++)
  {
    MsgAddr[I]=CurAddr;
    CurAddr+=pack(StrLength(CurAddr)+1);
  }

  for (int I=0;I<MsgCount;I++)
  {
    MsgAddrA[I]=CurAddrA;
    CurAddrA+=pack(strlen(CurAddrA)+1);
  }

  fclose(LangFile);
  SetLastError(LastError);
  if(this == &Lang)
    OldLang.Free();
  LanguageLoaded=TRUE;
  return(TRUE);
}


Language::~Language()
{
  Free();
}

void Language::Free()
{
  if(MsgList)xf_free(MsgList);
  if(MsgListA)xf_free(MsgListA);
  MsgList=NULL;
  MsgListA=NULL;
  if(MsgAddr)delete[] MsgAddr;
  MsgAddr=NULL;
  if(MsgAddrA)delete[] MsgAddrA;
  MsgAddrA=NULL;
  MsgCount=0;
  MsgSize=0;
}

void Language::Close()
{
  if(this == &Lang)
  {
    if(OldLang.MsgCount)
      OldLang.Free();
    OldLang.MsgList=MsgList;
    OldLang.MsgAddr=MsgAddr;
    OldLang.MsgListA=MsgListA;
    OldLang.MsgAddrA=MsgAddrA;
    OldLang.MsgCount=MsgCount;
    OldLang.MsgSize=MsgSize;
  }

  MsgList=NULL;
  MsgAddr=NULL;
  MsgListA=NULL;
  MsgAddrA=NULL;
  MsgCount=0;
  MsgSize=0;
  LanguageLoaded=FALSE;
}


void Language::ConvertString(const wchar_t *Src,string &strDest)
{
  wchar_t *Dest = strDest.GetBuffer ((int)wcslen (Src)*2);

  while (*Src)
    switch(*Src)
    {
      case L'\\':
        switch(Src[1])
        {
          case L'\\':
            *(Dest++)=L'\\';
            Src+=2;
            break;
          case L'\"':
            *(Dest++)=L'\"';
            Src+=2;
            break;
          case L'n':
            *(Dest++)=L'\n';
            Src+=2;
            break;
          case L'r':
            *(Dest++)=L'\r';
            Src+=2;
            break;
          case L'b':
            *(Dest++)=L'\b';
            Src+=2;
            break;
          case L't':
            *(Dest++)=L'\t';
            Src+=2;
            break;
          default:
            *(Dest++)=L'\\';
            Src++;
            break;
        }
        break;
      case L'"':
        *(Dest++)=L'"';
        Src+=(Src[1]==L'"') ? 2:1;
        break;
      default:
        *(Dest++)=*(Src++);
        break;
    }
  *Dest=0;

  strDest.ReleaseBuffer();
}

BOOL Language::CheckMsgId(int MsgId)
{
  /* $ 19.03.2002 DJ
     ��� ������������� ������� - ����� ������� ��������� �� ������
     (��� �����, ��� ���������)
  */
  if (MsgId>=MsgCount || MsgId < 0)
  {
    if(this == &Lang && !LanguageLoaded && this != &OldLang && OldLang.CheckMsgId(MsgId))
      return TRUE;

    /* $ 26.03.2002 DJ
       ���� �������� ��� � ����� - ��������� �� �������
    */
    if (!FrameManager->ManagerIsDown())
    {
      /* $ 03.09.2000 IS
         ! ���������� ��������� �� ���������� ������ � �������� �����
           (������ ��� ����� ���������� ������ � ����������� ������ ������ - �
           ����� �� ����� ������)
      */
      string strMsg1, strMsg2;
      strMsg1.Format(L"Incorrect or damaged %s", (const wchar_t*)strMessageFile);
      /* IS $ */
      strMsg2.Format(L"Message %d not found",MsgId);
      if (Message(MSG_WARNING,2,L"Error",strMsg1,strMsg2,L"Ok",L"Quit")==1)
        exit(0);
    }

    return FALSE;
  }
  return TRUE;
}

const wchar_t* Language::GetMsg (int nID)
{
  if( !CheckMsgId (nID) )
    return L"";

  if( this == &Lang && this != &OldLang && !LanguageLoaded && OldLang.MsgCount > 0)
    return(OldLang.MsgAddr[nID]);

  return(MsgAddr[nID]);
}

const char* Language::GetMsgA (int nID)
{
  if( !CheckMsgId (nID) )
    return "";

  if( this == &Lang && this != &OldLang && !LanguageLoaded && OldLang.MsgCount > 0)
    return(OldLang.MsgAddrA[nID]);

  return(MsgAddrA[nID]);
}


FILE* Language::OpenLangFile(const wchar_t *Path,const wchar_t *Mask,const wchar_t *Language, string &strFileName, int &nCodePage, BOOL StrongLang,string *pstrLangName)
{
  strFileName=L"";

  FILE *LangFile=NULL;
  string strFullName, strEngFileName;
  FAR_FIND_DATA_EX FindData;
  string strLangName;

  ScanTree ScTree(FALSE,FALSE);
  ScTree.SetFindPath(Path,Mask);
  while (ScTree.GetNextName(&FindData, strFullName))
  {
    strFileName = strFullName;
    if (Language==NULL)
      break;
    if ((LangFile=_wfopen(strFileName,L"rb"))==NULL)
      strFileName=L"";
    else
    {
      nCodePage = GetFileFormat (LangFile);

      string strNULL;

      if (GetLangParam(LangFile,L"Language",&strLangName,NULL, nCodePage) && StrCmpI(strLangName,Language)==0)
        break;
      fclose(LangFile);
      LangFile=NULL;
      if(StrongLang)
      {
        strFileName=strEngFileName=L"";
        break;
      }
      if (StrCmpI(strLangName,L"English")==0)
        strEngFileName = strFileName;
    }
  }

  if (LangFile==NULL)
  {
    if ( !strEngFileName.IsEmpty() )
      strFileName = strEngFileName;
    if ( !strFileName.IsEmpty() )
    {
      LangFile=_wfopen(strFileName,L"rb");
      if(pstrLangName)
         *pstrLangName=strLangName;
    }
  }

  return(LangFile);
}


int Language::GetLangParam(FILE *SrcFile,const wchar_t *ParamName,string *strParam1, string *strParam2, int nCodePage)
{
  wchar_t ReadStr[1024];

  string strFullParamName = L".";

  strFullParamName += ParamName;

  int Length=(int)strFullParamName.GetLength();
  /* $ 29.11.2001 DJ
     �� ������� ������� � �����; ������ @Contents �� ������
  */
  BOOL Found = FALSE;
  long OldPos = ftell (SrcFile);

  while ( ReadString (SrcFile, ReadStr, 1024, nCodePage)!=NULL)
  {
    if (StrCmpNI(ReadStr,strFullParamName,Length)==0)
    {
      wchar_t *Ptr=wcschr(ReadStr,L'=');
      if(Ptr)
      {
				*strParam1 = Ptr+1;
				wchar_t *EndPtr=strParam1->GetBuffer ();

				EndPtr = wcschr(EndPtr,L',');
        if ( strParam2 )
          *strParam2=L"";
        if (EndPtr!=NULL)
        {
          if (strParam2)
          {
            *strParam2 = EndPtr+1;
            RemoveTrailingSpaces(*strParam2);
          }
          *EndPtr=0;
        }

        strParam1->ReleaseBuffer();

        RemoveTrailingSpaces(*strParam1);
        Found = TRUE;
        break;
      }
    }
    else if (!StrCmpNI (ReadStr, L"@Contents", 9))
      break;
  }
  fseek (SrcFile,OldPos,SEEK_SET);

  return(Found);
}


int Language::Select(int HelpLanguage,VMenu **MenuPtr)
{
  const wchar_t *Title,*Mask;
  string *strDest;
  if (HelpLanguage)
  {
    Title=UMSG(MHelpLangTitle);
    Mask=HelpFileMask;
    strDest=&Opt.strHelpLanguage;
  }
  else
  {
    Title=UMSG(MLangTitle);
    Mask=LangFileMask;
    strDest=&Opt.strLanguage;
  }

  MenuItemEx LangMenuItem;

  LangMenuItem.Clear ();
  VMenu *LangMenu=new VMenu(Title,NULL,0,ScrY-4);
  *MenuPtr=LangMenu;
  LangMenu->SetFlags(VMENU_WRAPMODE);
  LangMenu->SetPosition(ScrX/2-8+5*HelpLanguage,ScrY/2-4+2*HelpLanguage,0,0);

  string strFullName;
  FAR_FIND_DATA_EX FindData;
  ScanTree ScTree(FALSE,FALSE);
  ScTree.SetFindPath(g_strFarPath, Mask);
  while (ScTree.GetNextName(&FindData,strFullName))
  {
    FILE *LangFile=_wfopen(strFullName,L"rb");
    if (LangFile==NULL)
      continue;

    int nCodePage = GetFileFormat(LangFile, NULL);

    string strLangName, strLangDescr;
    if (GetLangParam(LangFile,L"Language",&strLangName,&strLangDescr,nCodePage))
    {
       string strEntryName;
       if (!HelpLanguage || (!GetLangParam(LangFile,L"PluginContents",&strEntryName,NULL,nCodePage) &&
           !GetLangParam(LangFile,L"DocumentContents",&strEntryName,NULL,nCodePage)))
       {

         LangMenuItem.strName.Format(L"%.40s", !strLangDescr.IsEmpty() ? (const wchar_t*)strLangDescr:(const wchar_t*)strLangName);
         /* $ 01.08.2001 SVS
            �� ��������� ����������!
            ���� � ������� � ����� �������� ��� ���� HLF � �����������
            ������, ��... ����� ���������� ��� ������ �����.
         */
         if(LangMenu->FindItem(0,LangMenuItem.strName,LIFIND_EXACTMATCH) == -1)
         {
           LangMenuItem.SetSelect(StrCmpI(*strDest,strLangName)==0);
           LangMenu->SetUserData((void*)(const wchar_t*)strLangName,0,LangMenu->AddItem(&LangMenuItem));
         }
       }
    }
    fclose(LangFile);
  }
  LangMenu->AssignHighlights(FALSE);
  LangMenu->Process();
  if (LangMenu->Modal::GetExitCode()<0)
    return(FALSE);

  wchar_t *lpwszDest = strDest->GetBuffer(LangMenu->GetUserDataSize()/sizeof(wchar_t)+1);

  LangMenu->GetUserData(lpwszDest, LangMenu->GetUserDataSize());

  strDest->ReleaseBuffer();

  return(LangMenu->GetUserDataSize());
}

/* $ 01.09.2000 SVS
  + ����� �����, ��� ��������� ���������� ��� .Options
   .Options <KeyName>=<Value>
*/
int Language::GetOptionsParam(FILE *SrcFile,const wchar_t *KeyName,string &strValue, int nCodePage)
{
  wchar_t ReadStr[1024];

  string strFullParamName;

  wchar_t *Ptr;

  int Length=StrLength(L".Options");

  long CurFilePos=ftell(SrcFile);

  while ( ReadString (SrcFile, ReadStr, 1024, nCodePage) !=NULL)
  {
    if (!StrCmpNI(ReadStr,L".Options",Length))
    {
      strFullParamName = RemoveExternalSpaces(ReadStr+Length);

      Ptr = strFullParamName.GetBuffer ();

      if((Ptr=wcsrchr(Ptr,L'=')) == NULL)
      {
        strFullParamName.ReleaseBuffer ();
        continue;
      }

      *Ptr++=0;

      strValue = RemoveExternalSpaces(Ptr);

      strFullParamName.ReleaseBuffer ();

      RemoveExternalSpaces (strFullParamName);

      if (!StrCmpI(strFullParamName,KeyName))
        return(TRUE);
    }
  }
  fseek(SrcFile,CurFilePos,SEEK_SET);
  return(FALSE);
}

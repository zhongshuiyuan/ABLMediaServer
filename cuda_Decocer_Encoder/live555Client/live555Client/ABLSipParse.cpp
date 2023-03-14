#include "stdafx.h"
#include "ABLSipParse.h"

CABLSipParse::CABLSipParse()
{
	strcpy(szSplitStr[0], ";");
	strcpy(szSplitStr[1], ",");
}

//�ͷ�sip�ַ����ڴ� 
bool CABLSipParse::FreeSipString()
{
	SipFieldStruct * sipKey = NULL;
	for (SipFieldStructMap::iterator iterator1 = sipFieldValueMap.begin(); iterator1 != sipFieldValueMap.end(); ++iterator1)
	{
		sipKey = (*iterator1).second;

		delete sipKey;
		sipKey = NULL;
	}
	sipFieldValueMap.clear();

 	memset(szSipBodyContent, 0x00, sizeof(szSipBodyContent));
 	return true;
}

CABLSipParse::~CABLSipParse()
{
	std::lock_guard<std::mutex> lock(sipLock);
 	FreeSipString();
}
	
//��sipͷȫ��װ��map���棬�������ҳ���ϸ��������
bool CABLSipParse::ParseSipString(char* szSipString)
{
	std::lock_guard<std::mutex> lock(sipLock);

	FreeSipString();

	if (strlen(szSipString) <= 4)
		return false;

	SipFieldStruct * sipKey = NULL;
	SipFieldStruct * sipKey2;
	string           strSipStringFull = szSipString;
	int              nPosBody;

	for (SipFieldStructMap::iterator iterator1 = sipFieldValueMap.begin(); iterator1 != sipFieldValueMap.end(); ++iterator1)
	{
		sipKey = (*iterator1).second;
 		
		if (sipKey != NULL)
		{
		  delete sipKey;
		  sipKey = NULL;
 		}
 	}
	sipFieldValueMap.clear();

	//���ҳ�body����
	memset(szSipBodyContent, 0x00, sizeof(szSipBodyContent));
	nPosBody = strSipStringFull.find("\r\n\r\n", 0);
	if (nPosBody > 0 && strlen(szSipString) - (nPosBody + 4) > 0 && strlen(szSipString) - (nPosBody + 4) < MaxSipBodyContentLength)
	{//��body��������
		memcpy(szSipBodyContent, szSipString + nPosBody + 4, strlen(szSipString) - (nPosBody + 4));
		szSipString[nPosBody + 4] = 0x00;
	}

	string strSipString = szSipString;
	char   szLineString[1024] = { 0 };
	string strLineSting;
	string strFieldValue;

	sipKey = NULL;
	int nLineCount = 0;
	int nPos1 = 0 ,  nPos2 = 0,nPos3;
	while (true)
	{
		nPos2 = strSipString.find("\r\n", nPos1);
		if (nPos2 > 0 && nPos2 - nPos1 > 0 )
		{
			memset(szLineString, 0x00, sizeof(szLineString));
			memcpy(szLineString, szSipString + nPos1, nPos2 - nPos1);

			do
			{
 			  sipKey = new SipFieldStruct;
			} while (sipKey == NULL);

			strLineSting = szLineString;
			if (nLineCount == 0)
			{
				nPos3 = strLineSting.find(" ", 0);
				if (nPos3 > 0)
				{
					memcpy(sipKey->szKey, szLineString, nPos3);
					memcpy(sipKey->szValue, szLineString + nPos3 + 1, strlen(szLineString) - (nPos3 + 1));
				}
			}
			else
			{
				nPos3 = strLineSting.find(":", 0);
				if (nPos3 > 0)
				{
					memcpy(sipKey->szKey, szLineString, nPos3);
					memcpy(sipKey->szValue, szLineString + nPos3 + 1, strlen(szLineString) - (nPos3 + 1));
#if 1
					//����boost:string trim ����ȥ���ո�
					string strTrimLeft = sipKey->szValue;
					boost::trim(strTrimLeft);
					strcpy(sipKey->szValue, strTrimLeft.c_str());
#endif
				}
			}

			if (strlen(sipKey->szKey) > 0)
			{
				sipFieldValueMap.insert(SipFieldStructMap::value_type(sipKey->szKey, sipKey));

				//�ѷ�����Ϊһ���ؼ��֣�KEY���洢����
				if (nLineCount == 0)
				{
				   do
				   {
					sipKey2 = new SipFieldStruct;
				   } while (sipKey == NULL);

					strcpy(sipKey2->szKey, "Method");
					strcpy(sipKey2->szValue, sipKey->szKey);
					sipFieldValueMap.insert(SipFieldStructMap::value_type(sipKey2->szKey, sipKey2));
				}

				//��������
				int  nPos4=0, nPos5,nPos6;
				char             subKeyValue[512] = { 0 };
				string           strSubKeyValue;

				strFieldValue = sipKey->szValue;
				for (int i = 0; i < 2; i++)
				{//ѭ������2�Σ�һ�� ;��һ�� ,

					while (true)
					{
						nPos5 = strFieldValue.find(szSplitStr[i], nPos4);
						if (nPos5 > 0 && nPos5 - nPos4 > 0)
						{
							memset(subKeyValue, 0x00, sizeof(subKeyValue));
							memcpy(subKeyValue, sipKey->szValue + nPos4, nPos5 - nPos4);

							strSubKeyValue = subKeyValue;
							nPos6 = strSubKeyValue.find("=", 0);
							if (nPos6 > 0)
							{
							   do
							   {
								sipKey2 = new SipFieldStruct;
							   } while (sipKey == NULL);

								memcpy(sipKey2->szKey, subKeyValue, nPos6);
								memcpy(sipKey2->szValue, subKeyValue + nPos6 + 1, strlen(subKeyValue) - (nPos6 + 1));

#if 1
								//����boost:string trim ����ȥ���ո�
								string strTrimLeft = sipKey2->szKey;
								boost::trim(strTrimLeft);
								strcpy(sipKey2->szKey, strTrimLeft.c_str());

								//ɾ��˫����
								strTrimLeft = sipKey2->szValue;
								boost::erase_all(strTrimLeft, "\"");
								strcpy(sipKey2->szValue, strTrimLeft.c_str());
#endif
								sipFieldValueMap.insert(SipFieldStructMap::value_type(sipKey2->szKey, sipKey2));
							}
						}
						else
						{
							if (nPos4 > 0 && strstr(sipKey->szValue, szSplitStr[i]) != NULL && strlen(sipKey->szValue) > nPos4)
							{
								memset(subKeyValue, 0x00, sizeof(subKeyValue));
								memcpy(subKeyValue, sipKey->szValue + nPos4, strlen(sipKey->szValue) - nPos4);

								strSubKeyValue = subKeyValue;
								nPos6 = strSubKeyValue.find("=", 0);
								if (nPos6 > 0)
								{
									do
									{
									   sipKey2 = new SipFieldStruct;
								    } while (sipKey == NULL);

									memcpy(sipKey2->szKey, subKeyValue, nPos6);
									memcpy(sipKey2->szValue, subKeyValue + nPos6 + 1, strlen(subKeyValue) - (nPos6 + 1));

#if 1
									//����boost:string trim ����ȥ���ո�
									string strTrimLeft = sipKey2->szKey;
									boost::trim(strTrimLeft);
									strcpy(sipKey2->szKey, strTrimLeft.c_str());

									//ɾ��˫����
									strTrimLeft = sipKey2->szValue;
									boost::erase_all(strTrimLeft, "\"");
									strcpy(sipKey2->szValue, strTrimLeft.c_str());
#endif
									sipFieldValueMap.insert(SipFieldStructMap::value_type(sipKey2->szKey, sipKey2));
								}
							}

							break;
						}

						nPos4 = nPos5 + 1;
					}

				}//for (int i = 0; i < 2; i++)
			}
			else
			{
				if (sipKey)
				{
				  delete sipKey;
				  sipKey = NULL;
 				}
			}

			nPos1 = nPos2+2;
			nLineCount ++;
		}
		else
			break;
	}

	return true;
}

bool CABLSipParse::GetFieldValue(char* szKey, char* szValue)
{
	std::lock_guard<std::mutex> lock(sipLock);

	SipFieldStruct * sipKey = NULL;
	SipFieldStructMap::iterator iterator1 = sipFieldValueMap.find(szKey);

	if (iterator1 != sipFieldValueMap.end())
	{
		sipKey = (*iterator1).second;
		strcpy(szValue, sipKey->szValue);
		 
		return true;
	}
	else
		return false;
}

bool CABLSipParse::AddFieldValue(char* szKey, char* szValue)
{
	std::lock_guard<std::mutex> lock(vectorLock);

	SipFieldStruct  sipKey ;
	strcpy(sipKey.szKey, szKey);
	strcpy(sipKey.szValue, szValue);
	sipFieldValueVector.push_back(sipKey);

	return true;
}

bool CABLSipParse::GetFieldValueString(char* szSipString)
{
	std::lock_guard<std::mutex> lock(vectorLock);

	if (sipFieldValueVector.size() == 0)
		return false;

	int nSize = sipFieldValueVector.size();
	int i;
	for (i = 0; i < nSize; i++)
	{
		sprintf(szSipLineBuffer, "%s: %s\r\n", sipFieldValueVector[i].szKey, sipFieldValueVector[i].szValue);
		if (i == 0)
			strcpy(szSipString, szSipLineBuffer);
		else
			strcat(szSipString, szSipLineBuffer);
	}
	strcat(szSipString, "\r\n");
	sipFieldValueVector.clear();

	return true;
}

int   CABLSipParse::GetSize()
{
	std::lock_guard<std::mutex> lock(vectorLock);
	return sipFieldValueMap.size();
}
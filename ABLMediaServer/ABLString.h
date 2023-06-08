#pragma once
#include <stdio.h>
#include <string>
#include <cstring>
namespace ABL {

	std::string& trim(std::string& s);

	// ɾ���ַ�����ָ�����ַ���
	int	erase_all(std::string& strBuf, const std::string& strDel);

	//std::string to_lower(std::string strBuf);

	int	replace_all(std::string& strBuf, std::string  strSrc, std::string  strDes);
	
	bool is_digits(const std::string& str);

	// �ַ���תСд
	std::string 	StrToLwr(std::string  strBuf);

	void to_lower(char* str);

	void to_lower(std::string& str);


	// �ַ���ת��д
	std::string 	StrToUpr(std::string  strBuf);

	void to_upper(char* str);

	void to_upper(std::string& str);

}
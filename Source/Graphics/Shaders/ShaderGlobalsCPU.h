#pragma once

#include <fstream>
#include <string>



inline void LoadShaderByteCode(std::string&& shaderPath, _Out_ char*& shaderByteCode, _Out_ int& shaderByteCodeLength)
{
	std::ifstream is;
	is.open(shaderPath.c_str(), std::ios::binary);
	// Get the length of file:
	is.seekg(0, std::ios::end);
	shaderByteCodeLength = is.tellg();
	is.seekg(0, std::ios::beg);
	// Allocate memory:
	shaderByteCode = new char[shaderByteCodeLength];
	// Read the data
	is.read(shaderByteCode, shaderByteCodeLength);
	is.close();
}
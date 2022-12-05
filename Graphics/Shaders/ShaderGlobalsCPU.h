#pragma once

#include <fstream>

using namespace std;

inline void LoadShaderByteCode(const char* shaderPath, _Out_ char*& shaderByteCode, _Out_ int& shaderByteCodeLength)
{
	ifstream is;
	is.open(shaderPath, ios::binary);
	// Get the length of file:
	is.seekg(0, ios::end);
	shaderByteCodeLength = is.tellg();
	is.seekg(0, ios::beg);
	// Allocate memory:
	shaderByteCode = new char[shaderByteCodeLength];
	// Read the data
	is.read(shaderByteCode, shaderByteCodeLength);
	is.close();
}
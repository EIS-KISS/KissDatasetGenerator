#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <eisgenerator/eistype.h>

#include "hash.h"

class Example
{
public:
	std::vector<eis::DataPoint> data;
	size_t label;

	Example(const std::vector<eis::DataPoint>& dataIn, size_t labelIn):
	data(dataIn), label(labelIn)
	{

	}

	Example(){}

	uint64_t hash() const
	{
		return murmurHash64(data.data(), data.size(), 0);
	}
};

class EisDataset
{
public:
	virtual Example get(size_t index) = 0;
	virtual size_t classesCount() const = 0;
	virtual size_t size() const = 0;
	virtual size_t classForIndex(size_t index) = 0;
	virtual std::string modelStringForClass(size_t classNum) {return std::string("Unkown");}
	virtual bool isMulticlass() {return false;}
	virtual std::vector<int64_t> classCounts()
	{
		std::vector<int64_t> counts(classesCount(), 0);
		return counts;
	}
	virtual ~EisDataset(){}
};

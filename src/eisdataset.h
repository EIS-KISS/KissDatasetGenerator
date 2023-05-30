#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <eisgenerator/eistype.h>

#include "hash.h"

class EisDataset
{
private:
	virtual eis::EisSpectra getImpl(size_t index) = 0;

public:
	eis::EisSpectra get(size_t index);
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

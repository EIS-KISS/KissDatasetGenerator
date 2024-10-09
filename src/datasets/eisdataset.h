#pragma once
#include <cstdint>
#include <string>
#include <kisstype/ype.h>

class EisDataset
{
private:
	virtual eis::Spectra getImpl(size_t index) = 0;

public:
	eis::Spectra get(size_t index);
	virtual size_t size() const = 0;
	virtual size_t classForIndex(size_t index) = 0;
	virtual std::string modelStringForClass(size_t classNum) {return std::string("Unkown");}
	virtual std::string getDescription() {return "";};
	virtual ~EisDataset(){}
};

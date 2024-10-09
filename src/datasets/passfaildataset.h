#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <eistype.h>
#include <spectra.h>
#include <mutex>
#include <memory>

#include "eisdataset.h"
#include "randomgen.h"

class PassFaillDataset:
public EisDataset
{
	EisDataset* dataset_;
	std::shared_ptr<std::mutex> datasetMutex_;

private:
	void normalize(std::vector<eis::DataPoint> data)
	{
		eis::DataPoint max = *std::max_element(data.begin(), data.end());
		for(eis::DataPoint& dp : data)
			dp = dp / max;
	}

	void randomize(std::vector<eis::DataPoint>& data, double magnitude)
	{
		for(size_t i = 1; i < data.size()-1; ++i)
			data[i].im += std::complex<fvalue>((rd::rand(2)-1)*magnitude, (rd::rand(2)-1)*magnitude);
		normalize(data);
	}

	virtual eis::EisSpectra getImpl(size_t index) override
	{
		datasetMutex_->lock();
		eis::EisSpectra example = dataset_->get(index % dataset_->size());
		datasetMutex_->unlock();
		bool pass = true;
		if(index < dataset_->size())
		{
			pass = false;
			if(rd::rand() < 0.01)
			{
				for(eis::DataPoint& dp : example.data)
					dp.im = std::complex<fvalue>(rd::rand(), rd::rand());
				normalize(example.data);
			}
			else
			{
				double magnitude = rd::rand(0.02)+0.01;
				randomize(example.data, magnitude);
			}
		}

		if(pass)
			example.model = "Pass";
		else
			example.model = "Fail";
		return example;
	}

public:
	PassFaillDataset(EisDataset *dataset):
	dataset_(dataset)
	{
		datasetMutex_.reset(new std::mutex);
	}

	virtual size_t size() const override
	{
		return dataset_->size()*2;
	}

	virtual size_t classForIndex(size_t index) override
	{
		return index > dataset_->size();
	}

	virtual std::string modelStringForClass(size_t classNum) override
	{
		return classNum == 0 ? "Fail" : "Pass";
	}
};

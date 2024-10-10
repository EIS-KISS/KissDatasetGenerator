#pragma once

#include <kisstype/type.h>
#include <eisgenerator/model.h>
#include <string>

#include "eisdataset.h"

class ParameterRegressionDataset: public EisDataset
{
public:
	static constexpr size_t DEFAULT_EXAMPLE_COUNT = 1e8;

private:
	eis::Model model;

	eis::Range omega;
	double noise;
	size_t sweepCount;
	size_t parameterCount;
	bool drt;

private:
	virtual eis::Spectra getImpl(size_t index) override;
	static fvalue max(const std::vector<eis::DataPoint>& data);

public:
	explicit ParameterRegressionDataset(const std::string& model, int64_t desiredSize, int64_t outputSize = 100, double noiseI = 0, bool drtI = false);
	ParameterRegressionDataset(const ParameterRegressionDataset& in) = default;

	void setOmegaRange(eis::Range range);

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
	virtual size_t size() const override;
};

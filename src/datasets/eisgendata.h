#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <eisgenerator/model.h>
#include <string>
#include <filesystem>
#include <memory>

#include "eisdataset.h"
#include "spectra.h"

class EisGeneratorDataset :
public EisDataset
{
	struct ModelData
	{
		std::shared_ptr<eis::Model> model;
		size_t sweepSize;
		size_t classNum;

		bool isSingleSweep;
		std::string singleModelParamString;
		std::vector<eis::DataPoint> data;
	};

public:
	static constexpr bool PRINT = false;
	static constexpr size_t DEFAULT_EXAMPLE_COUNT = 1e8;

private:
	std::vector<ModelData> models;
	std::vector<std::string> modelStrings;

	eis::Range omega;
	double noise;

private:
	std::pair<size_t, size_t> getModelAndOffsetForIndex(size_t index) const;
	void addVectorOfModels(const std::vector<std::string>& modelStrs, int64_t desiredSize);

	virtual eis::Spectra getImpl(size_t index) override;

public:
	explicit EisGeneratorDataset(int64_t outputSize = 100, double noiseI = 0);
	explicit EisGeneratorDataset(const std::filesystem::path& path, int64_t desiredSize, int64_t outputSize, double noise);
	explicit EisGeneratorDataset(std::istream& is, int64_t outputSize, int64_t desiredSize, double noise);
	explicit EisGeneratorDataset(const char* cStr, size_t cStrLen, int64_t desiredSize, int64_t outputSize, double noise);

	EisGeneratorDataset(const EisGeneratorDataset& in) = default;
	void addModel(const eis::Model& model);
	void addModel(std::shared_ptr<eis::Model> model);
	EisGeneratorDataset* getTestDataset();
	size_t frequencies();
	void setOmegaRange(eis::Range range);
	size_t singleSweepCount();
	void compileModels();

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
	virtual size_t size() const override;
};

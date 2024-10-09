#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <eisgenerator/spectra.h>

#include "eisdataset.h"


class EisDirDataset : public EisDataset
{
private:

	struct FileNameStr
	{
		std::filesystem::path path;
		size_t classNum;
	};

	std::vector<EisDirDataset::FileNameStr> fileNames;
	size_t inputSize;
	std::vector<std::string> modelStrs;
	std::vector<std::string> selectLabels;
	std::vector<std::string> extraInputs;

	virtual eis::EisSpectra getImpl(size_t index) override;

public:
	explicit EisDirDataset(const std::string& dirName, int64_t inputSize = 100, std::vector<std::string> selectLabels = {}, std::vector<std::string> extraInputs = {});
	EisDirDataset(const EisDirDataset& in) = default;

	virtual size_t size() const override;

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
};

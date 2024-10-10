#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <kisstype/spectra.h>

#include "eisdataset.h"
#include "microtar.h"


class TarDataset : public EisDataset
{
private:

	mtar_t tar;

	struct File
	{
		std::filesystem::path path;
		size_t classNum;
		size_t pos;
		size_t size;
	};

	std::vector<TarDataset::File> files;
	size_t inputSize;
	std::vector<std::string> modelStrs;
	std::vector<std::string> selectLabels;
	std::vector<std::string> extraInputs;
	std::filesystem::path path;
	bool normalization;

	virtual eis::Spectra getImpl(size_t index) override;
	eis::Spectra loadSpectraAtCurrentPos(size_t size);

public:
	explicit TarDataset(const std::filesystem::path& path, int64_t inputSize = 100, std::vector<std::string> selectLabels = {}, std::vector<std::string> extraInputs = {}, bool normalization = true);
	TarDataset(const TarDataset& in);
	TarDataset& operator=(const TarDataset& in);
	~TarDataset();

	virtual size_t size() const override;

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
};

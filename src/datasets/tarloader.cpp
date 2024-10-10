#include "tarloader.h"

#include <algorithm>
#include <assert.h>
#include <kisstype/type.h>
#include <eisgenerator/translators.h>

#include "model.h"
#include "../log.h"

#include "filterdata.h"

TarDataset::TarDataset(const std::filesystem::path& path, int64_t inputSize, std::vector<std::string> selectLabels, std::vector<std::string> extraInputs, bool normalization):
inputSize(inputSize), selectLabels(selectLabels), extraInputs(extraInputs),path(path), normalization(normalization)
{
	int ret = mtar_open(&tar, path.c_str(), "r");
	if(ret)
	{
		Log(Log::ERROR)<<"Unable to open tar at "<<path;
		return;
	}

	mtar_header_t header;
	while((mtar_read_header(&tar, &header)) != MTAR_ENULLRECORD)
	{
		if(header.type == MTAR_TREG)
		{
			std::filesystem::path path = header.name;
			size_t pos = tar.pos;
			eis::Spectra spectra = loadSpectraAtCurrentPos(header.size);

			bool skip = false;
			for(const std::string& key : selectLabels)
			{
				if(!spectra.hasLabel(key))
				{
					Log(Log::INFO)<<"Dsicarding as it is missing: "<<key;
					skip = true;
					break;
				}
			}
			for(const std::string& key : extraInputs)
			{
				if(!spectra.hasLabel(key))
				{
					Log(Log::INFO)<<"Dsicarding as it is missing: "<<key;
					skip = true;
					break;
				}
				continue;
			}

			if(!skip)
			{
				eis::purgeEisParamBrackets(spectra.model);
				eis::Model::removeSeriesResitance(spectra.model);

				if(spectra.model.length() < 2 && spectra.model != "r" && spectra.model != "c" && spectra.model != "w" && spectra.model != "p" && spectra.model != "l")
					spectra.model = "Union";

				auto search = std::find(modelStrs.begin(), modelStrs.end(), spectra.model);
				size_t index;
				if(search == modelStrs.end())
				{
					index = modelStrs.size();
					modelStrs.push_back(spectra.model);
					Log(Log::DEBUG)<<"New model "<<index<<": "<<spectra.model;
				}
				else
				{
					index = search - modelStrs.begin();
				}
				files.push_back({.path = path, .classNum = index, .pos = pos, .size = header.size});
			}
		}
		mtar_next(&tar);
	}
	if(files.size() < 20)
		Log(Log::WARN)<<"found few valid files in "<<path;
}

eis::Spectra TarDataset::loadSpectraAtCurrentPos(size_t size)
{
	char* filebuffer = new char[size+1];
	filebuffer[size] = '\0';
	int ret = mtar_read_data(&tar, filebuffer, size);
	if(ret != 0)
	{
		Log(Log::ERROR)<<"Unable to read from tar archive";
		assert(ret == 0);
	}
	std::stringstream ss(filebuffer);

	eis::Spectra spectra = eis::Spectra::loadFromStream(ss);
	delete[] filebuffer;

	return spectra;
}

TarDataset::TarDataset(const TarDataset& in)
{
	operator=(in);
}

TarDataset& TarDataset::operator=(const TarDataset& in)
{
	files = in.files;
	inputSize = in.inputSize;
	modelStrs = in.modelStrs;
	selectLabels = in.selectLabels;
	extraInputs = in.extraInputs;
	path = in.path;
	normalization = in.normalization;
	int ret = mtar_open(&tar, path.c_str(), "r");
	if(ret != 0)
	{
		Log(Log::ERROR)<<"Unable to reopen tar file at "<<path;
		assert(ret == 0);
	}
	return *this;
}

TarDataset::~TarDataset()
{
	mtar_close(&tar);
}

eis::Spectra TarDataset::getImpl(size_t index)
{
	if(files.size() < index)
	{
		Log(Log::ERROR)<<"index "<<index<<" out of range in "<<__func__;
		assert(false);
		return {};
	}

	mtar_seek(&tar, files[index].pos);
	eis::Spectra spectra = loadSpectraAtCurrentPos(files[index].size);

	if(normalization)
		filterData(spectra.data, inputSize);

	if(!selectLabels.empty() || !extraInputs.empty())
	{
		eis::Spectra copy = spectra;
		spectra.labelNames.clear();
		spectra.labels.clear();
		for(const std::string& key : selectLabels)
			spectra.addLabel(key, copy.getLabel(key));
		for(const std::string& key : extraInputs)
			spectra.addLabel("exip_" + key, copy.getLabel(key));
	}

	return spectra;
}

size_t TarDataset::classForIndex(size_t index)
{
	return files[index].classNum;
}

size_t TarDataset::size() const
{
	return files.size();
}

std::string TarDataset::modelStringForClass(size_t classNum)
{
	if(classNum >= modelStrs.size())
		return "invalid";
	else
		return *std::next(modelStrs.begin(), classNum);
}

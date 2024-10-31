#include "dirloader.h"

#include <algorithm>
#include <assert.h>
#include <kisstype/type.h>
#include <eisgenerator/translators.h>

#include "model.h"
#include "../log.h"

#include "filterdata.h"

EisDirDataset::EisDirDataset(const std::string& dirName, int64_t inputSize, std::vector<std::string> selectLabels, std::vector<std::string> extraInputs, bool normalization):
inputSize(inputSize), selectLabels(selectLabels), extraInputs(extraInputs), normalization(normalization)
{
	const std::filesystem::path directoryPath{dirName};
	if(!std::filesystem::is_directory(dirName))
	{
		Log(Log::WARN)<<dirName<<" is not a valid directory";
		return;
	}

	for(const std::filesystem::directory_entry& dirent : std::filesystem::directory_iterator{directoryPath})
	{
		if(!dirent.is_regular_file() || dirent.path().extension() != ".csv")
			continue;
		Log(Log::DEBUG)<<"Using: "<<dirent.path().filename();
		eis::Spectra spectra = eis::Spectra::loadFromDisk(dirent.path());

		bool fail = false;
		for(const std::string& key : selectLabels)
		{
			if(!spectra.hasLabel(key))
			{
				Log(Log::DEBUG)<<"Dsicarding as it is missing: "<<key;
				fail = true;
				break;
			}
		}
		if(fail)
			continue;

		for(const std::string& key : extraInputs)
		{
			if(!spectra.hasLabel(key))
			{
				Log(Log::DEBUG)<<"Dsicarding as it is missing: "<<key;
				fail = true;
				break;
			}
			continue;
		}
		if(fail)
			continue;

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
		fileNames.push_back({dirent.path(), index});
	}
	if(fileNames.size() < 20)
		Log(Log::WARN)<<"found few valid files in "<<directoryPath;
}

size_t EisDirDataset::removeLessThan(size_t examples)
{
	std::vector<size_t> classCounts(modelStrs.size(), 0);
	for(FileNameStr file : fileNames)
		++classCounts[file.classNum];

	size_t removed = 0;

	Log(Log::DEBUG)<<"Class counts for removal:";
	for(size_t i =  0; i < classCounts.size(); ++i)
		Log(Log::DEBUG)<<modelStrs[i]<<": "<<classCounts[i]<<(classCounts[i] < examples ? "(removed)" : "");
	Log(Log::DEBUG, false)<<'\n';

	for(size_t i = 0; i < fileNames.size(); ++i)
	{
		if(classCounts[fileNames[i].classNum] < examples)
		{
			fileNames.erase(fileNames.begin()+i);
			--i;
			++removed;
		}
	}

	classCounts.assign(modelStrs.size(), 0);
	for(FileNameStr file : fileNames)
		++classCounts[file.classNum];

	Log(Log::DEBUG)<<"Class counts after removal:";
	for(size_t i =  0; i < classCounts.size(); ++i)
		Log(Log::DEBUG)<<modelStrs[i]<<": "<<classCounts[i]<<(classCounts[i] < examples ? "(removed)" : "");
	Log(Log::DEBUG, false)<<'\n';

	return removed;
}

eis::Spectra EisDirDataset::getImpl(size_t index)
{
	if(fileNames.size() < index)
	{
		Log(Log::ERROR)<<"index "<<index<<" out of range in "<<__func__;
		assert(false);
		return {};
	}

	eis::Spectra data;

	try
	{
		data = eis::Spectra::loadFromDisk(fileNames[index].path);
		eis::purgeEisParamBrackets(data.model);
		eis::Model::removeSeriesResitance(data.model);
		assert(modelStrs[fileNames[index].classNum] == data.model);
	}
	catch(const eis::file_error& err)
	{
		Log(Log::WARN)<<"Can't load datafile from "<<fileNames[index].path<<' '<<err.what();
		if(index != 0)
		{
			if(index+1 < size())
				return get(index+1);
			else
				return get(0);
		}
		assert(false);
	}

	filterData(data.data, inputSize, normalization);

	if(!selectLabels.empty() || !extraInputs.empty())
	{
		eis::Spectra copy = data;
		data.labelNames.clear();
		data.labels.clear();
		for(const std::string& key : selectLabels)
			data.addLabel(key, copy.getLabel(key));
		for(const std::string& key : extraInputs)
			data.addLabel("exip_" + key, copy.getLabel(key));
	}

	return data;
}

size_t EisDirDataset::classForIndex(size_t index)
{
	return fileNames[index].classNum;
}

size_t EisDirDataset::size() const
{
	return fileNames.size();
}

std::string EisDirDataset::modelStringForClass(size_t classNum)
{
	if(classNum >= modelStrs.size())
		return "invalid";
	else
		return *std::next(modelStrs.begin(), classNum);
}

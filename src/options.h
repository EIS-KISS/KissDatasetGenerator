/* * KissDatasetGenerator - A generator of datasets for TorchKissAnn
 * Copyright (C) 2025 Carl Klemm <carl@uvos.xyz>
 *
 * This file is part of KissDatasetGenerator.
 *
 * KissDatasetGenerator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KissDatasetGenerator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KissDatasetGenerator.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <string>
#include <argp.h>
#include <iostream>
#include <filesystem>
#include "log.h"

const char *argp_program_version = "kissdatasetgenerator";
const char *argp_program_bug_address = "<carl@uvos.xyz>";
static char doc[] = "Application that checkes and conditions a dataset in an eis dir";
static char args_doc[] = "";
#define DATASET_LIST "gen, passfail, regression, dir, tar"

typedef enum
{
	DATASET_INVALID = -1,
	DATASET_GEN = 0,
	DATASET_PASSFAIL,
	DATASET_REGRESSION,
	DATASET_DIR,
	DATASET_TAR
} DatasetMode;

static inline constexpr const char* datasetModeToStr(const DatasetMode mode)
{
	switch(mode)
	{
		case DATASET_GEN:
			return "gen";
		case DATASET_PASSFAIL:
			return "passfail";
		case DATASET_REGRESSION:
			return "regression";
		case DATASET_DIR:
			return "dir";
		case DATASET_TAR:
			return "tar";
		default:
			return "invalid";
	}
}

static inline DatasetMode parseDatasetMode(const std::string& in)
{
	if(in.empty() || in == datasetModeToStr(DATASET_GEN))
		return DATASET_GEN;
	else if(in == datasetModeToStr(DATASET_PASSFAIL))
		return DATASET_PASSFAIL;
	else if(in == datasetModeToStr(DATASET_REGRESSION))
		return DATASET_REGRESSION;
	else if(in == datasetModeToStr(DATASET_DIR))
		return DATASET_DIR;
	else if(in == datasetModeToStr(DATASET_TAR))
		return DATASET_TAR;
	return DATASET_INVALID;
}

struct Config
{
	std::filesystem::path datasetPath;
	std::filesystem::path outDir = "./out";
	std::string overrideModel;
	std::string extaInputs;
	std::string dataOptions;
	bool extraInputsSet = false;
	std::string selectLabels;
	std::string range;
	size_t frequencyCount = 100;
	bool selectLabelsSet = false;
	int testPercent = 0;
	DatasetMode mode = DATASET_INVALID;
	bool tar = true;
	bool saveImages = false;
	bool noNegative = false;
	bool printDatasetHelp = false;
};

static struct argp_option options[] =
{
  {"verbose",			'v', 0,				0,	"Show debug messages" },
  {"quiet",				'q', 0,				0,	"Show only errors" },
  {"dataset", 			'd', "[PATH]",		0,	"input dataset to use or the model string, in case of the regression purpose"},
  {"type", 				't', "[TYPE]",		0,	"type of dataset to export valid types: " DATASET_LIST},
  {"data-options",		's', "[OPTIONS,...]", 0, "Sets the options to be interpreted by the dataset"},
  {"help-dataset",		'h', 0, 			0,	"Prints the supported options for the given dataset type"},
  {"out-dir",			'o', "[PATH]",		0,	"directory where to export dataset"},
  {"test-percent",		'p', "[NUMBER]",	0,	"test dataset percentage"},
  {"no-archive",		'a', 0,				0,	"save as a dir instead of a tar arcive"},
  {"frequency-range",	'r', "[RANGE]",		0,	"Frequency range to simulate for simulated datasets"},
  {"frequency-count",	'c', "[NUMBER]",	0,	"the number of frequencies to simmulate, default: 100"},
  {"select-labels",	'l', "[LABLE1,LABEL2,...]", OPTION_ARG_OPTIONAL,	"select thiese labels to appear in the output dataset (requires them to be present in the input)"},
  {"extra-inputs",	'x', "[INPUT1,INPUT2,...]", OPTION_ARG_OPTIONAL,	"select thiese labels to appear in the output dataset as extra inputs (requires them to be present in the input)"},
  {"no-negative",		'g', 0,	0,	"remove examples with negative labels from the dataset"},
  {"images",			'i', 0,	0,	"save a plot for eatch spectra"},
  {"assign-model",		'z', "[MODEL]",	0,	"assign this model to all spectra"},
  { 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	Config *config = reinterpret_cast<Config*>(state->input);

	try
	{
		switch (key)
		{
		case 'q':
			Log::level = Log::ERROR;
			break;
		case 'v':
			Log::level = Log::DEBUG;
			break;
		case 'd':
			config->datasetPath = arg;
			break;
		case 'a':
			config->tar = false;
			break;
		case 'o':
			config->outDir = arg;
			break;
		case 'h':
			config->printDatasetHelp = true;
			break;
		case 'p':
			config->testPercent = std::stoi(std::string(arg));
			if(config->testPercent < 0 || config->testPercent > 100)
			{
				std::cout<<arg<<" passed for argument -"<<key<<" is not a valid percentage.";
				return ARGP_KEY_ERROR;
			}
			break;
		case 't':
			config->mode = parseDatasetMode(std::string(arg));
			break;
		case 'l':
			if(arg)
			{
				config->selectLabels = std::string(arg);
				if(config->selectLabels[0] == '=')
					config->selectLabels.erase(config->selectLabels.begin());
			}
			config->selectLabelsSet = true;
			break;
		case 'r':
			config->range.assign(arg);
			break;
		case 'x':
			if(arg)
			{
				config->extaInputs = std::string(arg);
				if(config->extaInputs[0] == '=')
					config->extaInputs.erase(config->extaInputs.begin());
			}
			config->selectLabelsSet = true;
			break;
		case 'c':
			config->frequencyCount = std::stoul(std::string(arg));
			break;
		case 's':
			config->dataOptions = arg;
			break;
		case 'g':
			config->noNegative = true;
			break;
		case 'i':
			config->saveImages = true;
			break;
		case 'z':
			config->overrideModel = arg;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
		}
	}
	catch(const std::invalid_argument& ex)
	{
		std::cout<<arg<<" passed for argument -"<<key<<" is not a valid number.";
		return ARGP_KEY_ERROR;
	}
	return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

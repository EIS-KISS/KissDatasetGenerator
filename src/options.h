#pragma once
#include <string>
#include <vector>
#include <argp.h>
#include <iostream>
#include <filesystem>
#include "log.h"

const char *argp_program_version = "conditioneisdirdataset";
const char *argp_program_bug_address = "<carl@uvos.xyz>";
static char doc[] = "Application that checkes and conditions a dataset in an eis dir";
static char args_doc[] = "";

static struct argp_option options[] =
{
  {"verbose",		'v', 0,			0,	"Show debug messages" },
  {"quiet",			'q', 0,			0,	"Show only errors" },
  {"dataset", 		'd', "[PATH]",	0,	"dataset to use or the model string, in case of the regression purpose" },
  {"purpose", 		'p', "[NAME]",	0,	"dataset type to use, classifiy or regression" },
  {"out-dir",		'o', "[PATH]",	0,	"directory where to export dataset"},
  {"test-percent",	't', "[NUMBER]",0,	"test dataset percentage"},
  {"archive",		'a', 0,			0,	"save as a tar archive instead of a directory"},
  {"size",			's', "[NUMBER]",0,	"size the dataset should have"},
  { 0 }
};

typedef enum {
	PURPOSE_CLASSFIY,
	PURPOSE_REGRESSION,
	PURPOSE_INVALID
} Purpose;

struct Config
{
	std::filesystem::path datasetPath;
	std::filesystem::path outDir = "./out";
	size_t desiredSize = 100000;
	int testPercent = 0;
	Purpose purpose = PURPOSE_CLASSFIY;
	bool tar = false;
};

static const char* purposeToStr(Purpose purpose)
{
	switch (purpose)
	{
		case PURPOSE_CLASSFIY:
			return "classifiy";
		case PURPOSE_REGRESSION:
			return "regression";
		default:
			return "invalid";
	}
}

static Purpose parsePurpose(const std::string& in)
{
	if(in == purposeToStr(PURPOSE_CLASSFIY))
		return PURPOSE_CLASSFIY;
	else if(in == purposeToStr(PURPOSE_REGRESSION))
		return PURPOSE_REGRESSION;

	return PURPOSE_INVALID;
}

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
			config->tar = true;
			break;
		case 'o':
			config->outDir = arg;
			break;
		case 's':
			config->desiredSize = std::stoi(std::string(arg));
			break;
		case 't':
			config->testPercent = std::stoi(std::string(arg));
			if(config->testPercent < 0 || config->testPercent > 100)
			{
				std::cout<<arg<<" passed for argument -"<<key<<" is not a valid percentage.";
				return ARGP_KEY_ERROR;
			}
			break;
		case 'p':
			config->purpose = parsePurpose(std::string(arg));
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

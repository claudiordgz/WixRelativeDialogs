#include "parser.h"

#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/String.h"

#include <boost/lexical_cast.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <iterator>
#include <sstream>
#include <iostream>
#include <array>
#include <fstream>
#include <cmath>

using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

std::map<std::string, std::string> LoadConfigFile(std::string const &fileName);
std::pair<std::string, int> GetVariableName(std::string const &rawString, std::string &remaining);
bool processVariables(std::string &currVar, std::map<std::string, std::string> &varMap);
void processRemaining(std::map<std::string, std::string> &varMap, std::vector<std::string> &noprocess);
bool replaceOne(std::string& str, const std::string& from, const std::string& to);
bool replaceInMap(std::string const& searchStr, std::string &completeStr, std::map<std::string, std::string> &varMap);
void replaceAll(std::string& str, const std::string& from, const std::string& to);
std::string loadDialog(std::string const &fileName);
std::string processDialog(std::string const &fileName, std::map<std::string, std::string> const &config, std::map<std::string, std::string> const &dlgTemplates);
std::string parseDlgElements(std::string const &dlg);
void processDlgFormula(std::string &dlg, std::size_t begin, std::size_t end);
std::string processDlgSubDlg(std::string const &dlg, std::map<std::string, std::string> const &dlgTemplates);
void loadConfigurationTemplates(std::string const &templateDir, std::string const &templateName, std::map<std::string, std::string> &dlgTemplates);
std::map<std::string, std::string> cleanMapString(std::string &keyName);
void convertStringToArgumentMap(std::string const &rawArguments, std::map<std::string, std::string> &argMap);

struct ConfigurationWixDialogs {
	std::map<std::string, std::string> templateMap;
	std::map<std::string, std::string> configVariables;
};

template<class T>
void ProcessDialogs(T begin, T end, std::string const& inputFilesDir, std::string const& outputDir, ConfigurationWixDialogs const &cfg) {
	for(auto it = begin; it != end; ++it) {
		std::string dlg(processDialog( inputFilesDir + *it, cfg.configVariables, cfg.templateMap));
		boost::property_tree::ptree pt;
		std::stringstream ss(dlg);
		boost::property_tree::read_xml(ss, pt, boost::property_tree::xml_parser::trim_whitespace );
		boost::property_tree::xml_writer_settings<std::string> settings('\t', 1);
		std::string fileName = outputDir + *it;
		boost::property_tree::write_xml(fileName, pt, std::locale(), settings);
	}
}

int main(int argc, char *argv[])
{
	ConfigurationWixDialogs cfg;
	cfg.configVariables = LoadConfigFile("../WixDialogConverter/resource/config.ini");
	std::vector<std::string> unprocessedStrs;
	unprocessedStrs.reserve(10);
	for(auto it = cfg.configVariables.begin(); it != cfg.configVariables.end(); ++it) {
		bool processAgain = processVariables(it->second, cfg.configVariables);
		if(processAgain) {
			unprocessedStrs.push_back(it->first);
		}
	}
	processRemaining(cfg.configVariables, unprocessedStrs);
	std::array<std::string, 11> files = {"EpicorWelcomeDlg.wxs", "InstallDirectoryDlg.wxs", "EpicorDiskCostDlg.wxs", 
		"EpicorBrowseDlg.wxs", "EpicorVerifyReadyDlg.wxs", "EpicorExitDlg.wxs", "EpicorMaintenanceTypeDlg.wxs",
		"EpicorProgressDlg.wxs" ,"EpicorMaintenanceWelcomeDlg.wxs", "EpicorResumeDlg.wxs", "EpicorUserExit.wxs"};
	std::array<std::string, 1> peds = {"PEDSInstallDirDlg.wxs"};
	std::array<std::string, 14> templateNames = {
		"Backgrounds.dlg",
		"ConditionalInstallingText.dlg",
		"FirstBtn.dlg",
		"IncludeMaintenanceControl.dlg",
		"LeftPanelCompleted.dlg",
		"LeftPanelWelcomeCollectingInstalling.dlg",
		"LowerRightBanner.dlg",
		"LowerRightButtons.dlg",
		"LowerRightTwoButtons.dlg",
		"NotConditionalInstallingText.dlg",
		"TitleAndTitleBackground.dlg",
		"TitleBackground.dlg",
		"NewDirectoryEditElement.dlg",
		"NewEditElement.dlg"
	};
	std::string templateDir("../WixDialogConverter/resource/templates/");
	for(auto it = templateNames.begin(); it != templateNames.end(); ++it) {
		loadConfigurationTemplates(templateDir, *it, cfg.templateMap);
	}
	ProcessDialogs(files.begin(), files.end(), "../WixDialogConverter/resource/EpicorUI/", "C:/Users/claudio.rodriguez/Documents/Visual Studio 2010/Projects/PEDSProject/EpicorUI/", cfg);
	ProcessDialogs(peds.begin(), peds.end(), "../WixDialogConverter/resource/PEDSUI/", "C:/Users/claudio.rodriguez/Documents/Visual Studio 2010/Projects/PEDSProject/PEDSData/", cfg);
    return EXIT_SUCCESS;
}

std::string processDialog(std::string const &fileName, std::map<std::string, std::string> const &config, std::map<std::string, std::string> const &dlgTemplates) {
	std::string dlg = loadDialog(fileName);
	dlg = processDlgSubDlg(dlg, dlgTemplates);

	for(auto it = config.begin(); it != config.end(); ++it) {
		std::string from = "[" + it->first + "]";
		std::string to = it->second;
		replaceAll(dlg, from, to);
	}
	dlg = parseDlgElements(dlg);
	return dlg;
}

void loadConfigurationTemplates(std::string const &templateDir, std::string const &templateName, std::map<std::string, std::string> &dlgTemplates) {
	std::string dlgTemplate = loadDialog(templateDir + templateName);
	dlgTemplates[templateName] = dlgTemplate;
}

std::string processDlgSubDlg(std::string const&dlg, std::map<std::string, std::string> const &dlgTemplates) {
	std::string returnStr(dlg);
	bool changed = false;
	for(std::string::const_iterator it = dlg.begin(); it != dlg.end(); ++it) {
		if((*it) == '<') {
			auto itr = it;
			std::advance(itr, 1);
			if(*itr == '<') {
				std::advance(itr, 1);
				std::size_t begin = std::distance(dlg.begin(), itr);
				std::size_t end = std::distance(dlg.begin(),  dlg.end());
				std::string copyStr = dlg.substr(begin, end);
				std::string::size_type newEnd = copyStr.find(">>");
				std::string mapStr = copyStr.substr(0, newEnd);
				std::string completeReplacingStr = mapStr;
				std::map<std::string, std::string> argumentMap = cleanMapString(mapStr);
				mapStr = Poco::trim(mapStr);
				auto it = dlgTemplates.find(mapStr);
				if(it != dlgTemplates.end()) {
					std::string replacingText = it->second;
					if(!argumentMap.empty()) {
						for(auto itr = argumentMap.begin(); itr != argumentMap.end(); ++itr) {
							std::string replacingStr = Poco::trim(itr->second);
							replaceAll(replacingText, "::" + itr->first + "::", replacingStr);
						}
					}
					replaceOne(returnStr, "<<" + completeReplacingStr + ">>", replacingText);
					changed = true;
				}
			}
		}
	}
	if(changed) {
		std::string delimiter("<<");
		std::string::size_type pos = returnStr.find(delimiter);
		while(pos != std::string::npos) {
			returnStr = processDlgSubDlg(returnStr, dlgTemplates);
			pos = returnStr.find(delimiter);
		}
	}
	return returnStr;
}

std::map<std::string, std::string> cleanMapString(std::string &keyName) {
	std::map<std::string, std::string> stringFromAndTo;
	std::string keyCopy = keyName;
	for(std::string::iterator it = keyCopy.begin(); it != keyCopy.end(); ++it) {
		if((*it) == ':') {
			auto itr = it, itr2 = it;
			++itr2;
			std::size_t endPos = std::distance(keyCopy.begin(), itr);
			std::size_t beginPosComplete = std::distance(keyCopy.begin(), itr2);
			std::size_t endPosComplete = std::distance(itr2, keyCopy.end());
			std::string arguments = keyCopy.substr(beginPosComplete, endPosComplete);
			convertStringToArgumentMap(arguments, stringFromAndTo);
			keyName = keyCopy.substr(0, endPos); 
			break;
		}
	}
	return stringFromAndTo;
}

void convertStringToArgumentMap(std::string const &rawArguments, std::map<std::string, std::string> &argMap) {
	std::string lRawArgs = rawArguments;
	bool Stop = false;
	for(std::string::iterator it = lRawArgs.begin(); it != lRawArgs.end(); ++it) {
		if((*it) != ' ' && (*it) != '\n' && (*it) != '\t') {
			std::string::size_type begin = std::distance(lRawArgs.begin(), it);
			std::string::size_type completeSize = std::distance(lRawArgs.begin(), lRawArgs.end());
			lRawArgs = lRawArgs.substr(begin, completeSize);
			std::string::size_type end = lRawArgs.find(",");
			std::string arg = lRawArgs.substr(0, end);
			std::string::size_type newBegin = end + 1;
			while(lRawArgs[newBegin] == ' ') {
				++newBegin;
			}
			std::string newRawArguments = lRawArgs.substr(newBegin, completeSize);
			end = newRawArguments.find(";");
			std::string argVal = newRawArguments.substr(0, end);
			++end;
			std::string::size_type newEnd = std::distance(newRawArguments.begin(), newRawArguments.end());
			newRawArguments = newRawArguments.substr(end, newEnd);
			argMap[arg] = argVal;
			if(newRawArguments.empty()) {
				break;
			} else {
				convertStringToArgumentMap(newRawArguments, argMap);
				break;
			}
		}
	}
}

std::string parseDlgElements(std::string const &dlg) {
	std::string returnDlg(dlg);
	for(std::string::iterator it = returnDlg.begin(); it != returnDlg.end(); ++it) {
		if((*it) == '{') {
			auto itr = it;
			std::advance(itr, 1);
			if(*itr != '\\') {
				std::size_t begin = std::distance(returnDlg.begin(), itr);
				std::size_t end = std::distance(returnDlg.begin(),  returnDlg.end());
				processDlgFormula(returnDlg, begin, end);
			}
		}
	}
	return returnDlg;
}

void processDlgFormula(std::string &dlg, std::size_t begin, std::size_t end) {	
	std::string copyStr = dlg.substr(begin, end);
	std::string::size_type newEnd = copyStr.find("}");
	std::string parsableStr = copyStr.substr(0, newEnd);
	copyStr = "{" + copyStr.substr(0, newEnd) + "}";
	Parser prs;
	char* result = 0;
	result = prs.parse(parsableStr.c_str());
	if(result != 0) {
		std::string finalStr(result);
		double i = boost::lexical_cast<double>(finalStr);
		i = std::floor(i);
		int fi = boost::math::iround(i); 
		finalStr = boost::lexical_cast<std::string>(fi);
		replaceOne(dlg, copyStr, finalStr);
	}
}

std::string loadDialog(std::string const &fileName) {
	std::ifstream in(fileName);
	string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return s;
}

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

void processRemaining(std::map<std::string, std::string> &varMap, std::vector<std::string> &noprocess) {
	std::vector<std::string> reprocess;
	reprocess.reserve(10);
	for(auto it = noprocess.begin(); it != noprocess.end(); ++it) {
		bool processAgain = processVariables(varMap[*it], varMap);
		if(processAgain) {
			reprocess.push_back(*it);
		}
	}
	if(!reprocess.empty()) {
		processRemaining(varMap, reprocess);
	}
}

bool processVariables(std::string &currVar, std::map<std::string, std::string> &varMap) {
	std::string currentVariable = currVar;
	std::string::size_type startIdx = currentVariable.find('{');
	bool processAgain = false;
	if(startIdx != std::string::npos) {
		std::string remainingStr("");
		currentVariable = currentVariable.substr(++startIdx, (currentVariable.find('}') - 1));
		std::string completeVariable = currentVariable;
		std::pair<std::string, int> variableTuple(GetVariableName(currentVariable, remainingStr));
		processAgain = replaceInMap(variableTuple.first, completeVariable, varMap);
		bool subProcess = false;
		for(std::size_t i = 0; i != variableTuple.second; ++i) {
			currentVariable = remainingStr;
			std::pair<std::string, int> vSubTuple(GetVariableName(currentVariable, remainingStr));
			subProcess = replaceInMap(vSubTuple.first, completeVariable, varMap);
			if(subProcess) {
				processAgain = subProcess;
			}
		}
		if(processAgain) {
			std::string lComplete = "{" + completeVariable + "}";
			currVar = lComplete;
		} else {
			Parser prs;
			char* result = 0;
            result = prs.parse(completeVariable.c_str());
			if(result != 0) {
				std::string finalStr(result);
				currVar = finalStr;
			}
		}
	} else {

	}
	return processAgain;
}

bool replaceInMap(std::string const& searchStr, std::string &completeStr, std::map<std::string, std::string> &varMap) {
	bool returnValue = false;
	auto it = varMap.find(searchStr);
	if(it != varMap.end()) {
		std::string found = varMap[searchStr];
		std::size_t foundIdx = found.find('{');
		if(foundIdx != std::string::npos) {
			returnValue = true;
		} else {
			replaceOne(completeStr, "[" + searchStr + "]", found);
		}
	}
	return returnValue;
}

bool replaceOne(std::string& str, const std::string& from, const std::string& to) {
    std::size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

std::pair<std::string, int> GetVariableName(std::string const &rawString, std::string &remaining) {
	std::string::size_type startIdx = 0, endIdx = 0, instances = 0;
	std::string returnVariable("");
	startIdx = rawString.find('[');
	instances = startIdx;
	endIdx = std::distance(rawString.begin(), rawString.end());
	if(startIdx != std::string::npos) {
		++startIdx;
		std::string::size_type lEnd = 0; 
		lEnd = rawString.find(']');
		if(endIdx != std::string::npos) {
			returnVariable = rawString.substr(startIdx, lEnd - startIdx);
			++lEnd;
			if(lEnd < endIdx) {
				remaining = rawString.substr(lEnd, endIdx);
			}
		} else {
			//error
		}
	} else {
		//finished
	}
	std::size_t returnPos=0;
	std::string localString = rawString;
	while(instances != std::string::npos) {
		++instances;
		if(instances < endIdx) {
			localString = localString.substr(instances+1, endIdx);
			instances = localString.find('[');
			if(instances != std::string::npos) {
				++returnPos;
			}
		} else {
			instances = std::string::npos;
		}
	}
	return std::make_pair(returnVariable, returnPos);
}


std::map<std::string, std::string> LoadConfigFile(std::string const &fileName) {
	AutoPtr<IniFileConfiguration> pConf;
	try {
		pConf = new IniFileConfiguration(fileName);
	} catch (Poco::Exception &e) {
		std::string crash(e.what());
	}
	Poco::Util::AbstractConfiguration::Keys keys;
	pConf->keys(keys);
	std::ostringstream oss;
	std::vector<std::string> names;
	names.reserve(keys.size());
	if(!keys.empty()) {
		for(auto it = keys.begin(); it != keys.end(); ++it) {
			std::copy(it, it, std::ostream_iterator<std::string>(oss, ""));
			oss << *it;
			names.push_back(oss.str());
			oss.str(std::string());
		}
	}
	std::map<std::string, std::string> keyStringVector;
	for(auto it = names.begin(); it != names.end(); ++it) {
		std::string name = *it;
		pConf->keys(name, keys);
		for(auto it = keys.begin(); it != keys.end(); ++it) {
			std::string query = name + "." + *it;
			try {
				keyStringVector[*it] = pConf->getString(query);
			} catch (Poco::NotFoundException &e) {
				Poco::Util::AbstractConfiguration::Keys lKeys;
				pConf->keys(query, lKeys);
				for(auto itr = lKeys.begin(); itr != lKeys.end(); ++itr) {
					std::string elementName = *it + "." + *itr;
					std::string query = name + "." + elementName;
					keyStringVector[elementName] = pConf->getString(query);
				}
			}
		}
	}
	return keyStringVector;
}
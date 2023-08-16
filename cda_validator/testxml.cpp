
#include <iostream>
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <thread>
#include <filesystem>
#include "rapidxml.hpp"

using namespace std;
using namespace chrono;
using namespace filesystem;

bool outputValidationError = false;
bool onlyParsing = false;

rapidxml::xml_document<> docs[20];

bool validateXMLWithRapidXML(string path, int instance) {
	const char* p = path.c_str();

	FILE* file = fopen(p, "rb");
	if (!file) {
		cout << "filed to open a file" << p << "\n";
		return false;
	}

	fseek(file, 0, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (!length) {
		cout << "file's lenght is 0" << "\n";
		fclose(file);
		return false;
	}

	char* buf = (char*)malloc(length + 1);
	fread(buf, length, 1, file); buf[length] = 0;
	fclose(file);

	bool isValid = true;
	rapidxml::xml_document<>& doc = docs[instance];
	try {
		doc.parse<0>(buf);
	}
	catch (rapidxml::parse_error& e) {
		cout << "Parse error: " << e.what() << endl << "At: " << e.where<char>() << endl;
		return false;
	}
	catch (rapidxml::validation_error& e) {
		cout << "Validation error: " << e.what() << endl;
		return false;
	}

	if (onlyParsing)
		return true;

	int iRealmCode = 0;
	int iTemplateId = 0;
	int iTitle = 0;
	int iEffectiveTime = 0;
	int iSetId = 0;
	int iRecordTarget = 0;

	// get the root node of the document
	auto root_node = doc.first_node();

	// loop through all child nodes of the root node
	for (auto node = root_node->first_node(); node; node = node->next_sibling()) {
		char* node_name = node->name();
		char* node_value = node->value();

		if (strcmp("realmCode", node_name) == 0) {
			iRealmCode++;
			auto attr = node->first_attribute("code");
			if (!attr || strcmp("AT", attr->value()) != 0) {
				if (outputValidationError)
					cout << "realmCode is not 'AT', tag number: " << iRealmCode << "\n";
				isValid = false;
			}
		}
		else if (strcmp("templateId", node_name) == 0) {
			iTemplateId++;
			auto attrRoot = node->first_attribute("root");
			auto attrAAName = node->first_attribute("assigningAuthorityName");
			if (!attrRoot || strcmp("1.2.40.0.34.11.3", attrRoot->value()) != 0 ||
				!attrAAName || strcmp("ELGA", attrAAName->value()) != 0) {
				if (outputValidationError)
					cout << "Error AuthorityName isnt matching in templateID, tag number: " << iTemplateId << "\n";
				isValid = false;
			}
		}
		else if (strcmp("effectiveTime", node_name) == 0) {
			iEffectiveTime++;
			auto attrValue = node->first_attribute("value");
			if (attrValue) {
				tm datetime_tm = {};
				istringstream datetime_ss(attrValue->value());
				datetime_ss >> get_time(&datetime_tm, "%Y%m%d%H%M%S+0200");
				auto datetime_tp = system_clock::from_time_t(mktime(&datetime_tm));

				if (system_clock::now() < datetime_tp) {
					if (outputValidationError)
						cout << "ERROR: Document Timestamp is in future, tag number: " << iEffectiveTime << "\n";
					isValid = false;
				}
			}
		}
		else if (strcmp("setId", node_name) == 0) {
			iSetId++;
			auto versionNode = node->next_sibling();
			auto attrValue = versionNode->first_attribute("value");
			if (iSetId % 2 != 0) {
				if (!attrValue || strcmp("1", attrValue->value()) != 0) {
					if (outputValidationError)
						cout << "ERROR: 1. setID not version 1, tag number: " << (iSetId + 1) / 2 << "\n";
					isValid = false;
				}
			}
			else {
				if (!attrValue || strcmp("2", attrValue->value()) != 0) {
					if (outputValidationError)
						cout << "ERROR: 2. setID not version 2, tag number: " << iSetId / 2 << "\n";
					isValid = false;
				}
			}
		} if (strcmp("recordTarget", node_name) == 0) {
			iRecordTarget++;
			auto addrNode = node->first_node("patientRole")->first_node("addr");
			auto streetAddressLineNode = addrNode->first_node("streetAddressLine");
			auto postalCodeNode = addrNode->first_node("postalCode");
			auto cityNode = addrNode->first_node("city");
			if (streetAddressLineNode && postalCodeNode && cityNode) {
				char* addrLine = streetAddressLineNode->value();
				char* addrPCode = postalCodeNode->value();
				char* addrCity = cityNode->value();
				if (*addrLine == 0)
				{
					if (outputValidationError)
						cout << "ERROR: streetAddressLine exists but the value is empty, tag number: " << iRecordTarget << "\n";
					isValid = false;
				}
				if (*addrPCode == 0)
				{
					if (outputValidationError)
						cout << "ERROR: postalCode exists but the value is empty, tag number: " << iRecordTarget << "\n";
					isValid = false;
				}
				if (*addrCity == 0)
				{
					if (outputValidationError)
						cout << "ERROR: city exists but the value is empty, tag number: " << iRecordTarget << "\n";
					isValid = false;
				}
			}
			else {
				if (outputValidationError)
					cout << "streetAddressLine, postalCode or city is missing, tag number: " << iRecordTarget << "\n";
				isValid = false;
			}
		}
	}

	cout << "Number of Records processed: " << iRecordTarget << "\n";

	free(buf);
	return isValid;

}

void validateFiles(const vector<path>& files, int instance) {
	for (const auto& file : files) {
		if (is_regular_file(file)) {
			cout << "Validating result:" << (validateXMLWithRapidXML(file.string(), instance) ? "true" : "false") << "\n";
		}
	}
}

int main(int argc, char** argv) {
	auto startTime = high_resolution_clock::now();
	path dir_path = argv[1];
	vector<path> files;

	for (int i = 2; i < argc; ++i) {
		if (strcmp("outputvalidation", argv[i]) == 0) {
			outputValidationError = true;
		}
		else if (strcmp("onlyparsing", argv[i]) == 0) {
			onlyParsing = true;
		}
	}

	for (const auto& entry : directory_iterator(dir_path)) {
		if (is_regular_file(entry)) {
			files.push_back(entry);
		}
	}

	int num_threads = thread::hardware_concurrency();
	vector<thread> threads;
	size_t chunk_size = files.size() / num_threads;
	size_t start = 0;

	for (int i = 0; i < num_threads; i++) {
		size_t end = (i == num_threads - 1) ? files.size() : start + chunk_size;
		threads.push_back(thread(validateFiles, vector<path>(files.begin() + start, files.begin() + end), i));
		start = end;
	}

	for (auto& thread : threads) {
		thread.join();
	}

	auto stopTime = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(stopTime - startTime);

	cout << "Number of files processed: " << files.size() << "\n";
	cout << "Time taken to execute: " << duration.count() << " ms\n";

	cout << "-----------------------------------" << endl;
	cout << "Output Validation: " << (outputValidationError ? "true" : "false") << endl;
	cout << "Only Parsing: " << (onlyParsing ? "true" : "false") << endl;

	return 0;
}


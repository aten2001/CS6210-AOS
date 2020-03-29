#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <utility>

struct ProductSpec {

	const std::string name_;
	
	ProductSpec(const std::string& name)
	: name_(name)
	{}
};

struct ProductQueryResult {
	
	struct Bid {
		double price_;
		std::string vendor_id_;
	};
	std::vector<Bid> bids_;
};


inline bool read_product_queries(std::vector<ProductSpec>& queries, const std::string& filename) {
	
	std::ifstream myfile (filename);
	if (myfile.is_open()) {
		std::string line;
		while (getline(myfile,line)) {
			queries.push_back(ProductSpec(line));
		}
		myfile.close();
	}
	else {
		std::cerr << "Failed to open file " << filename << std::endl;
		return false;
	}

	return true;
}	

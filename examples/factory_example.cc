/* -*- c++ -*- */
/* Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 * co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */

/*!
 * A simple console line application for running flowgraphs. Very useful to check
 * weather all the blocks in your flowgraph are supported or not.
 */

#include <fstream>
#include <thread>
#include <chrono>
#include <boost/program_options.hpp>
#include <flowgraph/flowgraph.h>

namespace po = boost::program_options;

int main(int argc, char **argv)
{
	po::options_description desc("Allowed options");
	desc.add_options()
		("grc-file", po::value<std::string>()->default_value("example.grc"), "GRC file")
	;

	po::positional_options_description p;
	p.add("grc-file", 1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	po::notify(vm);

	std::string path = vm["grc-file"].as<std::string>();

	std::cout << "Using GRC file: " <<  path << "\n";

	std::ifstream input(path);
	auto graph = flowgraph::make_flowgraph(input);

	graph->start();
	std::cout << "Graph started, sleep for 10 seconds...\n";

	std::this_thread::sleep_for(std::chrono::seconds(10));

	graph->stop();
	std::cout << "Stop requested, waiting...\n";

	graph->wait();
	std::cout << "Stopped.\n";

	return 0;
}

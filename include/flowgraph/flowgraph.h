/* -*- c++ -*- */
/*
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _FLOWGRAPH_FLOWGRAPH_H_
#define _FLOWGRAPH_FLOWGRAPH_H_

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include <gnuradio/types.h>
#include <gnuradio/runtime_types.h>
#include <gnuradio/top_block.h>
#include <gnuradio/analog/sig_source_f.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/throttle.h>

#include <digitizers/time_domain_sink.h>
#include <digitizers/post_mortem_sink.h>
#include <digitizers/extractor.h>
#include <digitizers/time_realignment_ff.h>
#include <digitizers/picoscope_3000a.h>
#include <digitizers/edge_trigger_ff.h>

#include <flowgraph/api.h>


namespace flowgraph {

static const std::string blocks_throttle_key      = "blocks_throttle";
static const std::string time_domain_sink_key     = "digitizers_time_domain_sink";
static const std::string post_mortem_sink_key     = "digitizers_post_mortem_sink";
static const std::string extractor_key            = "digitizers_extractor";
static const std::string time_realignment_key     = "digitizers_time_realignment_ff";
static const std::string picoscope_3000a_key      = "digitizers_picoscope_3000a";
static const std::string picoscope_4000a_key      = "digitizers_picoscope_4000a";
static const std::string picoscope_6000_key      = "digitizers_picoscope_6000";
static const std::string edge_trigger_key         = "digitizers_edge_trigger_ff";

static const std::vector<std::string> supported_block_types =
{
		{"variable"},
		{"analog_sig_source_x"},
		{"blocks_null_sink"},
		blocks_throttle_key,
		time_domain_sink_key,
		post_mortem_sink_key,
		extractor_key,
		picoscope_3000a_key,
		edge_trigger_key,
		time_realignment_key
};

static const std::vector<std::string> digitizer_keys =
{
        picoscope_3000a_key,
        picoscope_4000a_key,
        picoscope_6000_key
};

class FlowGraph
{
	struct FlowGraphEntry
	{
		gr::block_sptr block;
		std::string type;
	};

public:
	FlowGraph(const std::string &name) :
		d_top_block(gr::make_top_block(name))
	{
	}

	/*!
	 * \brief Add gr-block to the flowgraph.
	 */
	void add(const gr::block_sptr& block, const std::string &id, const std::string &type)
	{
		if (d_block_map.count(id))
		{
			throw std::invalid_argument("block with id " + id + " previously added!");
		}

		FlowGraphEntry entry = {block, type};
		d_block_map[id] = entry;
	}

	/*!
	 * \brief Wire two gr-blocks or hierarchical blocks together
	 */
	void connect(const std::string &src, int src_port,
			const std::string &dst, int dst_port)
	{
		if (!d_block_map.count(src) || !d_block_map.count(dst))
		{
			throw std::invalid_argument("src " + src + " or dst " + dst + " not found!");
		}

		d_top_block->connect(d_block_map[src].block, src_port, d_block_map[dst].block, dst_port);
	}

    /*!
     * Start the contained flowgraph.
     *
     * \param max_noutput_items the maximum number of output items
     * allowed for any block in the flowgraph; the noutput_items can
     * always be less than this, but this will cap it as a
     * maximum. Use this to adjust the maximum latency a flowgraph can
     * exhibit.
     */
    void start(int max_noutput_items=100000000)
    {
    	d_top_block->start(max_noutput_items);
    }


    /*!
     * Stop the running flowgraph.
     */
    void stop()
    {
    	d_top_block->stop();
    }


    /*!
     * Wait for a flowgraph to complete.
     */
    void wait()
    {
    	d_top_block->wait();
    }

    template <class Callable>
    void digitizers_apply(Callable callable)
    {
        for (const auto &elem : d_block_map) {
            if (std::find(digitizer_keys.begin(), digitizer_keys.end(), elem.second.type) != digitizer_keys.end()) {
                auto sptr = boost::dynamic_pointer_cast<gr::digitizers::digitizer_block>(elem.second.block);
                callable(elem.first, sptr.get());
            }
        }
    }

    template <class Callable>
    void time_domain_sinks_apply(Callable callable)
    {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == time_domain_sink_key) {
                auto sptr = boost::dynamic_pointer_cast<gr::digitizers::time_domain_sink>(elem.second.block);
                callable(elem.first, sptr.get());
            }
        }
    }

    template <class Callable>
    void time_realignment_apply(Callable callable)
    {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == time_realignment_key) {
                auto sptr = boost::dynamic_pointer_cast<gr::digitizers::time_realignment_ff>(elem.second.block);
                callable(elem.first, sptr.get());
            }
        }
    }

    gr::digitizers::time_domain_sink::sptr
    get_time_domain_sink(const std::string &id) const
    {
        auto it = d_block_map.find(id);
        if (it != d_block_map.end()) {
            return boost::dynamic_pointer_cast<gr::digitizers::time_domain_sink>(it->second.block);
        }

        return nullptr;
    }

    std::vector<gr::digitizers::post_mortem_sink::sptr>
    post_mortem_sinks() const
    {
        std::vector<gr::digitizers::post_mortem_sink::sptr> vec;

        for (const auto &elem : d_block_map) {
            if (elem.second.type == post_mortem_sink_key) {
                vec.push_back(boost::dynamic_pointer_cast<gr::digitizers::post_mortem_sink>(elem.second.block));
            }
        }

        return vec;
    }

    gr::digitizers::post_mortem_sink *
    get_post_mortem_sink(const std::string &signal_name) const
    {
        for (auto & n : d_block_map) {
            if (n.second.type == post_mortem_sink_key) {
                auto sink = dynamic_cast<gr::digitizers::post_mortem_sink *>(n.second.block.get());
                assert(sink);
                if (sink->get_metadata().name == signal_name) {
                    return sink;
                }
            }
        }

        return nullptr;
    }

    gr::digitizers::time_domain_sink::sptr get_ready_time_domain_sink() const
    {
        std::vector<gr::digitizers::time_domain_sink::sptr> vec;

        for (const auto &elem : d_block_map) {
            if (elem.second.type == time_domain_sink_key) {

                auto sink = boost::dynamic_pointer_cast<gr::digitizers::time_domain_sink>(elem.second.block);

                if (sink->is_data_rdy()) {
                    return sink;
                }
            }
        }

        return gr::digitizers::time_domain_sink::sptr {};
    }

    void post_timing_event(const std::string &event_code, int64_t event_timestamp, int64_t beam_in_timestamp,
            bool time_sync_only=true, bool realignment_required=false)
    {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == time_realignment_key) {
                auto block = boost::dynamic_pointer_cast<gr::digitizers::time_realignment_ff>(elem.second.block);
                block->add_timing_event(event_code, event_timestamp,
                        beam_in_timestamp, time_sync_only, realignment_required);
            }
        }
    }

    void add_realignment_event(int64_t actual_event_timestamp, int64_t beam_in_timestamp) {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == time_realignment_key) {
                auto block = boost::dynamic_pointer_cast<gr::digitizers::time_realignment_ff>(elem.second.block);
                block->add_realignment_event(actual_event_timestamp, beam_in_timestamp);
            }
        }
    }

private:
	gr::top_block_sptr d_top_block;
	std::map<std::string, FlowGraphEntry> d_block_map;

};



/*!
 * \brief Creates a flowgraph based on input stream.
 *
 * Optionally, hardware addresses can be mapped to something else if needed,
 * meaning the value of 'serial_number' property is replaced by the user
 * defined value.
 *
 *
 *
 *
 * \param input
 * \param hw_mapping
 *
 * Example:
 * \code
 * std::ifstream input("input.grc");
 * auto graph = make_flowgraph(input);
 * \endcode
 * \returns flowgraph (unique pointer)
 */
std::unique_ptr<FlowGraph> FLOWGRAPH_API make_flowgraph(std::istream &input,
		const std::map<std::string, std::string> &hw_mapping = {});

}


#endif /* _FLOWGRAPH_FLOWGRAPH_H_ */

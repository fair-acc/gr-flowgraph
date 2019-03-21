/* -*- c++ -*- */
/* Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 * co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
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

#include <digitizers/block_aggregation.h>
#include <digitizers/block_amplitude_and_phase.h>
#include <digitizers/block_demux.h>
#include <digitizers/block_scaling_offset.h>
#include <digitizers/block_spectral_peaks.h>
#include <digitizers/chi_square_fit.h>
#include <digitizers/decimate_and_adjust_timebase.h>
#include <digitizers/edge_trigger_ff.h>
#include <digitizers/demux_ff.h>
#include <digitizers/freq_sink_f.h>
#include <digitizers/function_ff.h>
#include <digitizers/interlock_generation_ff.h>
#include <digitizers/picoscope_3000a.h>
#include <digitizers/picoscope_4000a.h>
#include <digitizers/picoscope_6000.h>
#include <digitizers/post_mortem_sink.h>
#include <digitizers/signal_averager.h>
#include <digitizers/stft_algorithms.h>
#include <digitizers/stft_goertzl_dynamic_decimated.h>
#include <digitizers/time_domain_sink.h>
#include <digitizers/time_realignment_ff.h>
#include <digitizers/cascade_sink.h>
#include <digitizers/edge_trigger_receiver_f.h>
#include <digitizers/wr_receiver_f.h>

#include <flowgraph/api.h>


namespace flowgraph {

/* gnuradio itself */
static const std::string blocks_throttle_key              = "blocks_throttle";
static const std::string blocks_tag_share_key             = "blocks_tag_share";
static const std::string blocks_tag_debug_key             = "blocks_tag_debug";
static const std::string blocks_complex_to_float_key      = "blocks_complex_to_float";
static const std::string blocks_float_to_complex_key      = "blocks_float_to_complex";
static const std::string blocks_null_sink_key             = "blocks_null_sink";
static const std::string blocks_null_source_key           = "blocks_null_source";
static const std::string blocks_uchar_to_float_key        = "blocks_uchar_to_float";
static const std::string blocks_vector_to_stream_key      = "blocks_vector_to_stream";
static const std::string blocks_stream_to_vector_key      = "blocks_stream_to_vector";
static const std::string blocks_vector_to_streams_key     = "blocks_vector_to_streams";
static const std::string analog_sig_source_x_key          = "analog_sig_source_x";
static const std::string freq_xlating_fir_filter_xxx_key  = "freq_xlating_fir_filter_xxx";
static const std::string band_pass_filter_taps_key        = "variable_band_pass_filter_taps";

/* Digitizer */
static const std::string block_aggregation_key            = "digitizers_block_aggregation";
static const std::string block_amplitude_and_phase_key    = "digitizers_block_amplitude_and_phase";
static const std::string block_complex_to_mag_deg_key     = "digitizers_block_complex_to_mag_deg";
static const std::string block_demux_key                  = "digitizers_block_demux";
static const std::string block_scaling_offset_key         = "digitizers_block_scaling_offset";
static const std::string block_spectral_peaks_key         = "digitizers_block_spectral_peaks";
static const std::string cascade_sink_key                 = "digitizers_cascade_sink";
static const std::string chi_square_fit_key               = "digitizers_chi_square_fit";
static const std::string decimate_and_adjust_timebase_key = "digitizers_decimate_and_adjust_timebase";
static const std::string edge_trigger_ff_key              = "digitizers_edge_trigger_ff";
static const std::string edge_trigger_receiver_f_key      = "digitizers_edge_trigger_receiver_f";
static const std::string demux_ff_key                     = "digitizers_demux_ff";
static const std::string freq_sink_f_key                  = "digitizers_freq_sink_f";
static const std::string function_ff_key                  = "digitizers_function_ff";
static const std::string interlock_generation_ff_key      = "digitizers_interlock_generation_ff";
static const std::string picoscope_3000a_key              = "digitizers_picoscope_3000a";
static const std::string picoscope_4000a_key              = "digitizers_picoscope_4000a";
static const std::string picoscope_6000_key               = "digitizers_picoscope_6000";
static const std::string post_mortem_sink_key             = "digitizers_post_mortem_sink";
static const std::string signal_averager_key              = "digitizers_signal_averager";
static const std::string stft_algorithms_key              = "digitizers_stft_algorithms";
static const std::string stft_goertzl_dynamic_key         = "digitizers_stft_goertzl_dynamic";
static const std::string time_domain_sink_key             = "digitizers_time_domain_sink";
static const std::string time_realignment_key             = "digitizers_time_realignment_ff";
static const std::string wr_receiver_f_key                = "digitizers_wr_receiver_f";

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
		gr::basic_block_sptr block;
		std::string type;
	};

public:
	FlowGraph(const std::string &name) :
		d_top_block(gr::make_top_block(name)),
		d_started(false)
	{
	}

	/*!
	 * \brief Add gr-block to the flowgraph.
	 */
	void add(const gr::basic_block_sptr& block, const std::string &id, const std::string &type)
	{
		if (d_block_map.count(id))
		{
            std::ostringstream message;
            message << "Exception in " << __FILE__ << ":" << __LINE__ << ": block with id " << id << " previously added!";
            throw std::invalid_argument(message.str());
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
		if (!d_block_map.count(src))
		{
		     std::ostringstream message;
		     message << "Exception in " << __FILE__ << ":" << __LINE__ << ": src " << src  << " not found!";
		     throw std::invalid_argument(message.str());
		}
        if (!d_block_map.count(dst))
        {
             std::ostringstream message;
             message << "Exception in " << __FILE__ << ":" << __LINE__ << ": dst " << dst << " not found!";
             throw std::invalid_argument(message.str());
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
    	d_started = true;
    }


    /*!
     * Stop the running flowgraph.
     */
    void stop()
    {
    	d_top_block->stop();
    	d_started = false;
    }

    /*!
     * \brief Returns true if the flowgraph was started, else false.
     */
    bool was_started()
    {
        return d_started;
    }

    /*!
     * Wait for a flowgraph to complete.
     */
    void wait()
    {
    	d_top_block->wait();
    }

    std::vector<gr::digitizers::signal_metadata_t> getAllChannelMetaData()
    {
    	std::vector<gr::digitizers::signal_metadata_t> channelMetaCol;
        for (const auto &elem : d_block_map) {
            if (elem.second.type == cascade_sink_key) {
                auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(elem.second.block);
                for (auto const &sink: cascade->get_time_domain_sinks()) {
                    channelMetaCol.push_back(sink->get_metadata());
                }
            }
            else if (elem.second.type == time_domain_sink_key) {
                auto sink = boost::dynamic_pointer_cast<gr::digitizers::time_domain_sink>(elem.second.block);
                channelMetaCol.push_back(sink->get_metadata());
            }
            else if (elem.second.type == freq_sink_f_key) {
                auto sink = boost::dynamic_pointer_cast<gr::digitizers::freq_sink_f>(elem.second.block);
                channelMetaCol.push_back(sink->get_metadata());
            }
            else if (elem.second.type == post_mortem_sink_key) {
                auto sink = boost::dynamic_pointer_cast<gr::digitizers::post_mortem_sink>(elem.second.block);
                channelMetaCol.push_back(sink->get_metadata());
            }
            else
            {
            	// ignore other blocks
            }
        }
        return channelMetaCol;
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
            else if (elem.second.type == cascade_sink_key) {
                auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(elem.second.block);
                for (auto const &sink: cascade->get_time_domain_sinks()) {
                  callable(elem.first + "_" + sink->get_metadata().name, sink.get());
                }
            }
        }
    }

    template <class Callable>
    void freq_sinks_apply(Callable callable)
    {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == freq_sink_f_key) {
                auto sptr = boost::dynamic_pointer_cast<gr::digitizers::freq_sink_f>(elem.second.block);
                callable(elem.first, sptr.get());
            }
            else if (elem.second.type == cascade_sink_key) {
                auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(elem.second.block);
                for (auto const &sink: cascade->get_frequency_domain_sinks()) {
                  callable(elem.first + "_" + sink->get_metadata().name, sink.get());
                }
            }
        }
    }

    template <class Callable>
    void post_mortem_sinks_apply(Callable callable)
    {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == post_mortem_sink_key) {
                auto sptr = boost::dynamic_pointer_cast<gr::digitizers::post_mortem_sink>(elem.second.block);
                callable(elem.first, sptr.get());
            }
            else if (elem.second.type == cascade_sink_key) {
                auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(elem.second.block);
                for (auto const &sink: cascade->get_post_mortem_sinks()) {
                    callable(elem.first + "_" + sink->get_metadata().name, sink.get());
                }
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

    template <class Callable>
    void interlock_apply(Callable callable)
    {
        for (const auto &elem : d_block_map) {
            if (elem.second.type == interlock_generation_ff_key) {
                auto sptr = boost::dynamic_pointer_cast<gr::digitizers::interlock_generation_ff>(elem.second.block);
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
        else {
            for (const auto &elem : d_block_map) {
                if (elem.second.type == cascade_sink_key) {
                    auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(elem.second.block);
                    for (auto const &sink: cascade->get_time_domain_sinks()) {
                        if(elem.first + "_" + sink->get_metadata().name == id) {
                            return sink;
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    template <class Block>
    boost::shared_ptr<Block>
    get_block(const std::string &id) const
    {
        auto it = d_block_map.find(id);
        if (it != d_block_map.end()) {
            return boost::dynamic_pointer_cast<Block>(it->second.block);
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
            else if (elem.second.type == cascade_sink_key) {
                auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(elem.second.block);
                for (auto const &pm: cascade->get_post_mortem_sinks()) {
                    vec.push_back(pm);
                }
            }
        }

        return vec;
    }

    gr::digitizers::post_mortem_sink::sptr
    get_post_mortem_sink(const std::string &signal_name) const
    {
        for (auto & n : d_block_map) {
            if (n.second.type == post_mortem_sink_key) {
                auto sink = boost::dynamic_pointer_cast<gr::digitizers::post_mortem_sink>(n.second.block);
                assert(sink);
                if (sink->get_metadata().name == signal_name) {
                    return sink;
                }
            }
            else if (n.second.type == cascade_sink_key) {
                auto cascade = boost::dynamic_pointer_cast<gr::digitizers::cascade_sink>(n.second.block);
                for (auto const &sink: cascade->get_post_mortem_sinks()) {
                    if (sink->get_metadata().name == signal_name) {
                        return sink;
                    }
                }
            }
        }

        return nullptr;
    }

    bool post_timing_event(const std::string &event_code, int64_t wr_trigger_stamp, int64_t wr_trigger_stamp_utc)
    {
        bool success = true;
        for (const auto &elem : d_block_map) {
            if (elem.second.type == time_realignment_key) {
                auto block = boost::dynamic_pointer_cast<gr::digitizers::time_realignment_ff>(elem.second.block);
                if( block->add_timing_event(event_code, wr_trigger_stamp, wr_trigger_stamp_utc) == false )
                        success = false;
            }
        }
        return success;
    }

private:
	gr::top_block_sptr d_top_block;
	std::map<std::string, FlowGraphEntry> d_block_map;
	bool d_started;

};


/*!
 * \brief Creates a flowgraph based on input stream.
 *
 * Optionally, hardware addresses can be mapped to something else if needed,
 * meaning the value of 'serial_number' property is replaced by the user
 * defined value.
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

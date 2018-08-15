/* -*- c++ -*- */
/*
 * Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 *
 * Co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */

#include <functional>
#include <memory>
#include <vector>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/type_traits.hpp>

#include "flowgraph_impl.h"

#include <gnuradio/analog/sig_source_f.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/null_source.h>
#include <gnuradio/blocks/throttle.h>
#include <gnuradio/blocks/stream_to_vector.h>
#include <gnuradio/blocks/vector_to_streams.h>
#include <gnuradio/blocks/tag_share.h>
#include <gnuradio/blocks/tag_debug.h>
#include <gnuradio/blocks/uchar_to_float.h>

namespace flowgraph {


std::ostream& operator<<(std::ostream& os, const BlockInfo& dt)
{
    os << "id/key: " << dt.id << "/"  << dt.key;

    if (dt.params.size()) {
    	os << ", params:\n";
		for (const auto& kv : dt.params) {
			os << "  " << kv.first << " : " << kv.second << "\n";
		}
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, const ConnectionInfo& dt)
{
    os << dt.src_id << ":" << dt.src_key << " <---> "
       << dt.dst_id << ":" << dt.dst_key;
    return os;
}

void GrcParser::parse()
{
    // populate tree structure
    boost::property_tree::ptree tree;
    boost::property_tree::read_xml(d_is, tree);

    boost::property_tree::ptree flow_graph = tree.get_child("flow_graph");

    BOOST_FOREACH( boost::property_tree::ptree::value_type const& f, flow_graph) {
        if (f.first == "block" ) {
            BlockInfo block;

            // Obtain key and parameters
            boost::property_tree::ptree subtree = (boost::property_tree::ptree) f.second;

            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, subtree) {
                if(v.first == "param") {
                    std::string key = v.second.get<std::string>("key");
                    std::string value = v.second.get<std::string>("value");

                    if (key == "id")
                    {
                        block.id = value;
                    } else {
                        block.params[key] = value;
                    }
                } else if (v.first == "key") {
                    block.key = v.second.get<std::string>("");
                } else {
                    std::cout << "unknown block entry: " << v.first << ", skipping...\n";
                }
            }

            if (block.key == "options") {
                d_top_block = block;
            }
            else if (block.key == "variable") {
                d_variables.push_back(block);
            }
            else {
                d_blocks.push_back(block);
            }
        } else if (f.first == "connection") {
            ConnectionInfo con;

            con.src_id = f.second.get<std::string>("source_block_id");
            con.dst_id = f.second.get<std::string>("sink_block_id");
            con.src_key = f.second.get<int>("source_key");
            con.dst_key = f.second.get<int>("sink_key");

            d_connections.push_back(con);

        } else {
            std::cout << "unknown flowgraph entry: " << f.first << ", skipping...\n";
        }
    }

    d_parsed = true;
}

void GrcParser::collapse_variables()
{
    // For simplicity make a map
    std::map<std::string, std::string> variable_value_map;
    for (const auto &v: d_variables) {
        variable_value_map[v.id] = v.param_value("value");
    }

    // If orig. value is in the map, replace it with the associated value
    for(auto& b: d_blocks) {
        for (auto& p: b.params) {
            if (variable_value_map.count(p.second)) {
                p.second = variable_value_map[p.second];
            }
        }
    }
}

int BlockMaker::getSizeOfType(std::string type)
{
    if (type == "complex")
        return sizeof(gr_complex);
    else if (type == "float")
        return sizeof(float);
    else if (type == "int")
        return sizeof(int);
    else if (type == "short")
        return sizeof(short);
    else if (type == "byte")
        return sizeof(char);
    else
    {
        std::ostringstream message;
        message << "Exception in " << __FILE__ << ":" << __LINE__ << ": invalid type: " << type;
        throw std::invalid_argument(message.str());
    }
}

// gnuradio blocks

struct NullSinkMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == "blocks_null_sink");

        auto type = info.param_value<>("type");
        auto vlen = info.eval_param_value<int>("vlen", variables);
        return gr::blocks::null_sink::make(vlen * getSizeOfType(type));
    }
};

struct NullSourceMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == "blocks_null_source");

        auto type = info.param_value<>("type");
        auto vlen = info.eval_param_value<int>("vlen", variables);
        return gr::blocks::null_source::make(vlen * getSizeOfType(type));
    }
};

struct UcharToFloatMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == "blocks_uchar_to_float");
        return gr::blocks::uchar_to_float::make();
    }
};

struct VectorToStreamMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == "blocks_vector_to_stream");

        auto type = info.param_value<>("type");
        auto num_items = info.eval_param_value<int>("num_items", variables);
        auto vlen      = info.eval_param_value<int>("vlen", variables);
        return gr::blocks::vector_to_stream::make(vlen * getSizeOfType(type), num_items);
    }
};

struct VectorToStreamsMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == "blocks_vector_to_streams");

        auto type = info.param_value<>("type");
        auto num_streams = info.eval_param_value<int>("num_streams", variables);
        auto vlen        = info.eval_param_value<int>("vlen", variables);
        return gr::blocks::vector_to_streams::make(vlen * getSizeOfType(type), num_streams);
    }
};

struct StreamToVectorMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == "blocks_stream_to_vector");

        auto type = info.param_value<>("type");
        auto num_items = info.eval_param_value<int>("num_items", variables);
        auto vlen      = info.eval_param_value<int>("vlen", variables);
        return gr::blocks::stream_to_vector::make(vlen * getSizeOfType(type), num_items);
    }
};

struct SigSourceMaker : BlockMaker
{
	const std::map<std::string, gr::analog::gr_waveform_t> enum_repr =
	{
		{"analog.GR_CONST_WAVE", gr::analog::GR_CONST_WAVE},
		{"analog.GR_SIN_WAVE", gr::analog::GR_SIN_WAVE},
		{"analog.GR_COS_WAVE", gr::analog::GR_COS_WAVE},
		{"analog.GR_SQR_WAVE", gr::analog::GR_SQR_WAVE},
		{"analog.GR_TRI_WAVE", gr::analog::GR_TRI_WAVE},
		{"analog.GR_SAW_WAVE", gr::analog::GR_SAW_WAVE},
	};

    gr::analog::gr_waveform_t lexical_cast(const std::string & s)
    {
    	return enum_repr.find(s)->second;
    }

	gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
	{
		assert(info.key == "analog_sig_source_x");
		auto sampling_freq = info.eval_param_value<double>("samp_rate", variables);
		auto wave_freq     = info.eval_param_value<double>("freq", variables);
		auto ampl          = info.eval_param_value<double>("amp", variables);
		auto offset        = info.eval_param_value<double>("offset", variables);
		auto waveform_type = lexical_cast(info.param_value<>("waveform"));

		return gr::analog::sig_source_f::make(sampling_freq, waveform_type, wave_freq, ampl, offset);
	}
};

struct ThrottleMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == blocks_throttle_key);

        auto type = info.param_value<>("type");
        auto samples_per_sec = info.eval_param_value<double>("samples_per_second", variables);
        auto ignore_tags = info.param_value<bool>("ignoretag");
        return gr::blocks::throttle::make(getSizeOfType(type), samples_per_sec, ignore_tags);
    }
};

struct TagShareMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == blocks_tag_share_key);

        auto io_type = info.param_value<>("io_type");
        auto share_type = info.param_value<>("share_type");
        auto vlen = info.eval_param_value<int>("vlen", variables);
        return gr::blocks::tag_share::make(getSizeOfType(io_type), getSizeOfType(share_type), vlen);
    }
};

struct TagDebugMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == blocks_tag_debug_key);

        auto type = info.param_value<>("type");
        auto name = info.param_value<>("name");
        auto filter = info.param_value<>("filter");
        auto vlen = info.eval_param_value<int>("vlen", variables);
        auto display = info.param_value<bool>("display");

        auto block = gr::blocks::tag_debug::make(getSizeOfType(type) * vlen, name, filter);
        block->set_display(display);
        return block;
    }
};

// digitizer blocks

struct AggregationMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == block_aggregation_key);

     auto alg_id    = info.eval_param_value<int>("alg_id", variables);
     auto decim    = info.eval_param_value<int>("decim", variables);
     auto delay    = info.eval_param_value<int>("delay", variables);
     auto fir_taps    = info.eval_param_vector<float>("fir_taps", variables);
     auto low_freq    = info.eval_param_value<double>("low_freq", variables);
     auto up_freq    = info.eval_param_value<double>("up_freq", variables);
     auto tr_width    = info.eval_param_value<double>("tr_width", variables);
     auto fb_user_taps    = info.eval_param_vector<double>("fb_user_taps", variables);
     auto fw_user_taps    = info.eval_param_vector<double>("fw_user_taps", variables);
     auto samp_rate    = info.eval_param_value<double>("samp_rate", variables);

     auto block =  gr::digitizers::block_aggregation::make(alg_id, decim, delay, fir_taps, low_freq, up_freq, tr_width, fb_user_taps, fw_user_taps, samp_rate);

     return block;
  }
};

struct AmplitudeAndPhaseMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == block_amplitude_and_phase_key);
     auto samp_rate    = info.eval_param_value<double>("samp_rate", variables);
     auto delay    = info.eval_param_value<double>("delay", variables);
     auto decim    = info.eval_param_value<int>("decim", variables);
     auto gain    = info.eval_param_value<double>("gain", variables);
     auto cutoff    = info.eval_param_value<double>("cutoff", variables);
     auto tr_width    = info.eval_param_value<double>("tr_width", variables);
     auto hil_win    = info.eval_param_value<int>("hil_win", variables);

     auto block =  gr::digitizers::block_amplitude_and_phase::make(samp_rate, delay, decim, gain, cutoff, tr_width, hil_win);

     return block;
  }
};

struct ComplexToMagDegMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == block_complex_to_mag_deg_key);
        auto vec_size = info.eval_param_value<int>("vec_size", variables);
        return gr::digitizers::block_complex_to_mag_deg::make(vec_size);
    }
};

struct DemuxMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == block_demux_key);

     auto bit_to_keep    = info.eval_param_value<double>("bit_to_keep", variables);

     auto block =  gr::digitizers::block_demux::make(bit_to_keep);

     return block;
  }
};

struct ScalingOffsetMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == block_scaling_offset_key);

     auto scale    = info.eval_param_value<double>("scale", variables);
     auto offset    = info.eval_param_value<double>("offset", variables);

     auto block =  gr::digitizers::block_scaling_offset::make(scale, offset);

     return block;
  }
};

struct SpectralPeaksMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == block_spectral_peaks_key);

     auto samp_rate  = info.eval_param_value<double>("samp_rate", variables);
     auto fft_win    = info.eval_param_value<int>("fft_win", variables);
     auto med_n      = info.eval_param_value<int>("med_n", variables);
     auto avg_n      = info.eval_param_value<int>("avg_n", variables);
     auto prox_n     = info.eval_param_value<int>("prox_n", variables);

     auto block =  gr::digitizers::block_spectral_peaks::make(samp_rate, fft_win, med_n, avg_n, prox_n);

     return block;
  }
};

struct CascadeSinkMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == cascade_sink_key);

        auto alg_id    = info.eval_param_value<int>("alg_id", variables);
        auto delay    = info.eval_param_value<int>("delay", variables);
        auto fir_taps    = info.eval_param_vector<float>("fir_taps", variables);
        auto low_freq    = info.eval_param_value<double>("low_freq", variables);
        auto up_freq    = info.eval_param_value<double>("up_freq", variables);
        auto tr_width    = info.eval_param_value<double>("tr_width", variables);
        auto fb_user_taps    = info.eval_param_vector<double>("fb_user_taps", variables);
        auto fw_user_taps    = info.eval_param_vector<double>("fw_user_taps", variables);
        auto samp_rate    = info.eval_param_value<double>("samp_rate", variables);
        auto pm_buffer    = info.eval_param_value<float>("pm_buffer", variables);
        auto signal_name = info.param_value("signal_name");
        auto signal_unit = info.param_value("signal_unit");

        return gr::digitizers::cascade_sink::make(alg_id, delay, fir_taps, low_freq, up_freq, tr_width, fb_user_taps,
            fw_user_taps, samp_rate, pm_buffer, signal_name, signal_unit);

    }
};
struct ChiSquareFitMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == chi_square_fit_key);

     auto num_samps    = info.eval_param_value<int>("num_samps", variables);
     auto function    = info.param_value("function");
     auto fun_u    = info.eval_param_value<double>("fun_u", variables);
     auto fun_l    = info.eval_param_value<double>("fun_l", variables);
     auto num_params    = info.eval_param_value<int>("num_params", variables);
     auto par_names    = info.param_value("par_names");
     auto param_init    = info.eval_param_vector<double>("param_init", variables);
     auto param_err    = info.eval_param_vector<double>("param_err", variables);
     auto param_fit    = info.eval_param_vector<int>("param_fit", variables);
     auto par_sp_l    = info.eval_param_vector<double>("par_sp_l", variables);
     auto par_sp_u    = info.eval_param_vector<double>("par_sp_u", variables);
     auto chi_sq    = info.eval_param_value<double>("chi_sq", variables);

     auto block =  gr::digitizers::chi_square_fit::make(num_samps, function, fun_u, fun_l,
             num_params, par_names, param_init, param_err, param_fit, par_sp_u, par_sp_l, chi_sq);

     return block;
  }
};

struct DecimateAndAdjustTimebaseMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == decimate_and_adjust_timebase_key);

     auto decimation  = info.eval_param_value<int>("decimation", variables);
     auto delay       = info.eval_param_value<double>("delay", variables);

     auto block =  gr::digitizers::decimate_and_adjust_timebase::make(decimation, delay);

     return block;
  }
};

struct EdgeTriggerMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == edge_trigger_ff_key);

        auto sampling          = info.eval_param_value<float>("sampling", variables);
        auto timeout           = info.eval_param_value<float>("timeout", variables);
        auto lo                = info.eval_param_value<float>("lo", variables);
        auto hi                = info.eval_param_value<float>("hi", variables);
        auto initial_satate    = info.eval_param_value<float>("initial_state", variables);
        auto send_udp          = info.param_value<bool>("send_udp");
        auto host_list         = info.param_value("host_list");

        auto block =  gr::digitizers::edge_trigger_ff::make(sampling, lo, hi, initial_satate, send_udp, host_list, timeout);
        return block;
    }
};

struct EdgeTriggerReceiverMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == edge_trigger_receiver_f_key);
        auto addr = info.param_value("addr");
        auto port = info.eval_param_value<int>("port", variables);
        return gr::digitizers::edge_trigger_receiver_f::make(addr, port);
    }
};


struct ExtractorMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
      assert(info.key == demux_ff_key);

      auto samp_rate           = info.eval_param_value<float>("samp_rate", variables);
      auto history             = info.eval_param_value<unsigned>("history", variables);
      auto pre_trigger_window  = info.eval_param_value<unsigned>("pre_trigger_window", variables);
      auto post_trigger_window = info.eval_param_value<unsigned>("post_trigger_window", variables);

      auto block =  gr::digitizers::demux_ff::make(samp_rate, history, post_trigger_window, pre_trigger_window);

      return block;
  }
};

struct FreqSinkMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == freq_sink_f_key);

     auto acquisition_type = info.eval_param_value<int>("acquisition_type", variables);
     auto signal_name      = info.param_value("signal_name");
     auto samp_rate        = info.eval_param_value<float>("samp_rate", variables);
     auto nbins            = info.eval_param_value<int>("nbins", variables);
     auto nmeasurements    = info.eval_param_value<int>("nmeasurements", variables);
     auto nbuffers         = info.eval_param_value<int>("nbuffers", variables);

     auto block =  gr::digitizers::freq_sink_f::make(signal_name, samp_rate, nbins, nmeasurements,
             nbuffers, gr::digitizers::freq_sink_mode_t(acquisition_type));

     return block;
  }
};

struct FunctionMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == function_ff_key);

        auto decimation  = info.eval_param_value<int>("decimation", variables);
        auto time        = info.eval_param_vector<float>("time", variables);
        auto reference   = info.eval_param_vector<float>("reference", variables);
        auto min         = info.eval_param_vector<float>("min", variables);
        auto max         = info.eval_param_vector<float>("max", variables);

        auto block = gr::digitizers::function_ff::make(decimation);

        block->set_function(time, reference, min, max);

        return block;
    }
};

struct InterlockGenerationMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == interlock_generation_ff_key);

     auto max_max    = info.eval_param_value<double>("max_max", variables);
     auto max_min    = info.eval_param_value<double>("max_min", variables);

     auto block =  gr::digitizers::interlock_generation_ff::make(max_min, max_max);

     return block;
  }
};

struct Ps3000aMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == picoscope_3000a_key);

        auto serial_number         = info.param_value("serial_number");
        auto trigger_once          = info.param_value<bool>("trigger_once");
        auto samp_rate             = info.eval_param_value<double>("samp_rate", variables);
        auto samples               = info.eval_param_value<int>("samples", variables);
        auto pre_samples           = info.eval_param_value<int>("pre_samples", variables);
        auto downsampling_mode     = info.param_value<int>("downsampling_mode");
        auto downsampling_factor   = info.eval_param_value<int>("downsampling_factor", variables);

        auto ps = gr::digitizers::picoscope_3000a::make(serial_number, true);
        ps->set_trigger_once(trigger_once);
        ps->set_samp_rate(samp_rate);
        ps->set_samples(samples, pre_samples);
        ps->set_downsampling(
                static_cast<gr::digitizers::downsampling_mode_t>(downsampling_mode),
                downsampling_factor);

        auto enable_ai_a = info.param_value<bool>("enable_ai_a");
        if (enable_ai_a) {
            auto range_ai_a = info.param_value<double>("range_ai_a");
            auto coupling_ai_a = info.param_value<bool>("coupling_ai_a");
            auto offset_ai_a = info.param_value<double>("offset_ai_a");
            ps->set_aichan("A", enable_ai_a, range_ai_a, coupling_ai_a, offset_ai_a);
        }

        auto enable_ai_b = info.param_value<bool>("enable_ai_b");
        if (enable_ai_b) {
            auto range_ai_b = info.param_value<double>("range_ai_b");
            auto coupling_ai_b = info.param_value<bool>("coupling_ai_b");
            auto offset_ai_b = info.param_value<double>("offset_ai_b");
            ps->set_aichan("B", enable_ai_b, range_ai_b, coupling_ai_b, offset_ai_b);
        }

        auto enable_ai_c = info.param_value<bool>("enable_ai_c");
        if (enable_ai_c) {
            auto range_ai_c = info.param_value<double>("range_ai_c");
            auto coupling_ai_c = info.param_value<bool>("coupling_ai_c");
            auto offset_ai_c = info.param_value<double>("offset_ai_c");
            ps->set_aichan("C", enable_ai_c, range_ai_c, coupling_ai_c, offset_ai_c);
        }

        auto enable_ai_d = info.param_value<bool>("enable_ai_d");
        if (enable_ai_d) {
            auto range_ai_d = info.param_value<double>("range_ai_d");
            auto coupling_ai_d = info.param_value<bool>("coupling_ai_d");
            auto offset_ai_d = info.param_value<double>("offset_ai_d");
            ps->set_aichan("D", enable_ai_d, range_ai_d, coupling_ai_d, offset_ai_d);
        }

        auto enable_di_0 = info.param_value<bool>("enable_di_0");
        auto thresh_di_0 = info.param_value<double>("thresh_di_0");
        ps->set_diport("port0", enable_di_0, thresh_di_0);

        auto enable_di_1 = info.param_value<bool>("enable_di_1");
        auto thresh_di_1 = info.param_value<double>("thresh_di_1");
        ps->set_diport("port1", enable_di_1, thresh_di_1);

        auto trigger_source = info.param_value("trigger_source");

        if (trigger_source != "None") {
            if (trigger_source == "Digital") {
                auto pin_number = info.param_value<uint32_t>("pin_number");
                auto trigger_direction = info.param_value<int>("trigger_direction");
                ps->set_di_trigger(pin_number,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction));
            }
            else {
                auto trigger_direction = info.param_value<int>("trigger_direction");
                auto trigger_threshold = info.param_value<double>("trigger_threshold");
                ps->set_aichan_trigger(
                        trigger_source,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction),
                        trigger_threshold);
            }
        }

        auto acquisition_mode = info.param_value("acquisition_mode");

        if (acquisition_mode == "Streaming") {
            auto buff_size = info.eval_param_value<int>("buff_size", variables);
            auto poll_rate = info.eval_param_value<int>("poll_rate", variables);
            ps->set_buffer_size(buff_size);
            ps->set_streaming(poll_rate);
        }
        else {
            auto nr_waveforms = info.eval_param_value<int>("nr_waveforms", variables);
            ps->set_rapid_block(nr_waveforms);
        }

        return ps;
    }
};

struct Ps4000aMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == picoscope_4000a_key);

        auto serial_number         = info.param_value("serial_number");
        auto trigger_once          = info.param_value<bool>("trigger_once");
        auto samp_rate             = info.eval_param_value<double>("samp_rate", variables);
        auto samples               = info.eval_param_value<int>("samples", variables);
        auto pre_samples           = info.eval_param_value<int>("pre_samples", variables);
        auto downsampling_mode     = info.param_value<int>("downsampling_mode");
        auto downsampling_factor   = info.eval_param_value<int>("downsampling_factor", variables);

        auto ps = gr::digitizers::picoscope_4000a::make(serial_number, true);
        ps->set_trigger_once(trigger_once);
        ps->set_samp_rate(samp_rate);
        ps->set_samples(samples, pre_samples);
        ps->set_downsampling(
                static_cast<gr::digitizers::downsampling_mode_t>(downsampling_mode),
                downsampling_factor);

        auto enable_ai_a = info.param_value<bool>("enable_ai_a");
        if (enable_ai_a) {
            auto range_ai_a    = info.param_value<double>("range_ai_a");
            auto coupling_ai_a = info.param_value<bool>("coupling_ai_a");
            auto offset_ai_a   = info.param_value<double>("offset_ai_a");
            ps->set_aichan("A", enable_ai_a, range_ai_a, coupling_ai_a, offset_ai_a);
        }

        auto enable_ai_b = info.param_value<bool>("enable_ai_b");
        if (enable_ai_b) {
            auto range_ai_b = info.param_value<double>("range_ai_b");
            auto coupling_ai_b = info.param_value<bool>("coupling_ai_b");
            auto offset_ai_b = info.param_value<double>("offset_ai_b");
            ps->set_aichan("B", enable_ai_b, range_ai_b, coupling_ai_b, offset_ai_b);
        }

        auto enable_ai_c = info.param_value<bool>("enable_ai_c");
        if (enable_ai_c) {
            auto range_ai_c = info.param_value<double>("range_ai_c");
            auto coupling_ai_c = info.param_value<bool>("coupling_ai_c");
            auto offset_ai_c = info.param_value<double>("offset_ai_c");
            ps->set_aichan("C", enable_ai_c, range_ai_c, coupling_ai_c, offset_ai_c);
        }

        auto enable_ai_d = info.param_value<bool>("enable_ai_d");
        if (enable_ai_d) {
            auto range_ai_d = info.param_value<double>("range_ai_d");
            auto coupling_ai_d = info.param_value<bool>("coupling_ai_d");
            auto offset_ai_d = info.param_value<double>("offset_ai_d");
            ps->set_aichan("D", enable_ai_d, range_ai_d, coupling_ai_d, offset_ai_d);
        }

        auto enable_ai_e = info.param_value<bool>("enable_ai_e");
        if (enable_ai_e) {
            auto range_ai_e = info.param_value<double>("range_ai_e");
            auto coupling_ai_e = info.param_value<bool>("coupling_ai_e");
            auto offset_ai_e = info.param_value<double>("offset_ai_e");
            ps->set_aichan("E", enable_ai_e, range_ai_e, coupling_ai_e, offset_ai_e);
        }

        auto enable_ai_f = info.param_value<bool>("enable_ai_f");
        if (enable_ai_f) {
            auto range_ai_f = info.param_value<double>("range_ai_f");
            auto coupling_ai_f = info.param_value<bool>("coupling_ai_f");
            auto offset_ai_f = info.param_value<double>("offset_ai_f");
            ps->set_aichan("F", enable_ai_f, range_ai_f, coupling_ai_f, offset_ai_f);
        }

        auto enable_ai_g = info.param_value<bool>("enable_ai_g");
        if (enable_ai_g) {
            auto range_ai_g = info.param_value<double>("range_ai_g");
            auto coupling_ai_g = info.param_value<bool>("coupling_ai_g");
            auto offset_ai_g = info.param_value<double>("offset_ai_g");
            ps->set_aichan("G", enable_ai_g, range_ai_g, coupling_ai_g, offset_ai_g);
        }

        auto enable_ai_h = info.param_value<bool>("enable_ai_h");
        if (enable_ai_h) {
            auto range_ai_h = info.param_value<double>("range_ai_h");
            auto coupling_ai_h = info.param_value<bool>("coupling_ai_h");
            auto offset_ai_h = info.param_value<double>("offset_ai_h");
            ps->set_aichan("H", enable_ai_h, range_ai_h, coupling_ai_h, offset_ai_h);
        }


        auto trigger_source = info.param_value("trigger_source");

        if (trigger_source != "None") {
            if (trigger_source == "Digital") {
                auto pin_number = info.param_value<uint32_t>("pin_number");
                auto trigger_direction = info.param_value<int>("trigger_direction");
                ps->set_di_trigger(pin_number,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction));
            }
            else {
                auto trigger_direction = info.param_value<int>("trigger_direction");
                auto trigger_threshold = info.param_value<double>("trigger_threshold");
                ps->set_aichan_trigger(
                        trigger_source,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction),
                        trigger_threshold);
            }
        }

        auto acquisition_mode = info.param_value("acquisition_mode");

        if (acquisition_mode == "Streaming") {
            auto buff_size = info.eval_param_value<int>("buff_size", variables);
            auto poll_rate = info.eval_param_value<int>("poll_rate", variables);
            ps->set_buffer_size(buff_size);
            ps->set_streaming(poll_rate);
        }
        else {
            auto nr_waveforms = info.eval_param_value<int>("nr_waveforms", variables);
            ps->set_rapid_block(nr_waveforms);
        }

        return ps;
    }
};

struct Ps6000Maker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == picoscope_6000_key);

        auto serial_number         = info.param_value("serial_number");
        auto trigger_once          = info.param_value<bool>("trigger_once");
        auto samp_rate             = info.eval_param_value<double>("samp_rate", variables);
        auto samples               = info.eval_param_value<int>("samples", variables);
        auto pre_samples           = info.eval_param_value<int>("pre_samples", variables);
        auto downsampling_mode     = info.param_value<int>("downsampling_mode");
        auto downsampling_factor   = info.eval_param_value<int>("downsampling_factor", variables);

        auto ps = gr::digitizers::picoscope_6000::make(serial_number, true);
        ps->set_trigger_once(trigger_once);
        ps->set_samp_rate(samp_rate);
        ps->set_samples(samples, pre_samples);
        ps->set_downsampling(
                static_cast<gr::digitizers::downsampling_mode_t>(downsampling_mode),
                downsampling_factor);

        auto enable_ai_a = info.param_value<bool>("enable_ai_a");
        if (enable_ai_a) {
            auto range_ai_a = info.param_value<double>("range_ai_a");
            auto coupling_ai_a = info.param_value<bool>("coupling_ai_a");
            auto offset_ai_a = info.param_value<double>("offset_ai_a");
            ps->set_aichan("A", enable_ai_a, range_ai_a, coupling_ai_a, offset_ai_a);
        }

        auto enable_ai_b = info.param_value<bool>("enable_ai_b");
        if (enable_ai_b) {
            auto range_ai_b = info.param_value<double>("range_ai_b");
            auto coupling_ai_b = info.param_value<bool>("coupling_ai_b");
            auto offset_ai_b = info.param_value<double>("offset_ai_b");
            ps->set_aichan("B", enable_ai_b, range_ai_b, coupling_ai_b, offset_ai_b);
        }

        auto enable_ai_c = info.param_value<bool>("enable_ai_c");
        if (enable_ai_c) {
            auto range_ai_c = info.param_value<double>("range_ai_c");
            auto coupling_ai_c = info.param_value<bool>("coupling_ai_c");
            auto offset_ai_c = info.param_value<double>("offset_ai_c");
            ps->set_aichan("C", enable_ai_c, range_ai_c, coupling_ai_c, offset_ai_c);
        }

        auto enable_ai_d = info.param_value<bool>("enable_ai_d");
        if (enable_ai_d) {
            auto range_ai_d = info.param_value<double>("range_ai_d");
            auto coupling_ai_d = info.param_value<bool>("coupling_ai_d");
            auto offset_ai_d = info.param_value<double>("offset_ai_d");
            ps->set_aichan("D", enable_ai_d, range_ai_d, coupling_ai_d, offset_ai_d);
        }
        auto trigger_source = info.param_value("trigger_source");

        if (trigger_source != "None") {
            if (trigger_source == "Digital") {
                auto pin_number = info.param_value<uint32_t>("pin_number");
                auto trigger_direction = info.param_value<int>("trigger_direction");
                ps->set_di_trigger(pin_number,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction));
            }
            else {
                auto trigger_direction = info.param_value<int>("trigger_direction");
                auto trigger_threshold = info.param_value<double>("trigger_threshold");
                ps->set_aichan_trigger(
                        trigger_source,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction),
                        trigger_threshold);
            }
        }

        auto acquisition_mode = info.param_value("acquisition_mode");

        if (acquisition_mode == "Streaming") {
            auto buff_size = info.eval_param_value<int>("buff_size", variables);
            auto poll_rate = info.eval_param_value<int>("poll_rate", variables);
            ps->set_buffer_size(buff_size);
            ps->set_streaming(poll_rate);
        }
        else {
            auto nr_waveforms = info.eval_param_value<int>("nr_waveforms", variables);
            ps->set_rapid_block(nr_waveforms);
        }

        return ps;
    }
};

struct PostMortemSinkMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == post_mortem_sink_key);

        auto signal_name = info.param_value("signal_name");
        auto signal_unit = info.param_value("signal_unit");
        auto samp_rate = info.eval_param_value<float>("samp_rate", variables);
        auto buffer_size = info.param_value<int>("buffer_size");

        auto sink =  gr::digitizers::post_mortem_sink::make(signal_name, signal_unit, samp_rate, buffer_size);
        return sink;
    }
};

struct SignalAveragerMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == signal_averager_key);

     auto window_size    = info.eval_param_value<int>("window_size", variables);
     auto n_ports    = info.eval_param_value<int>("n_ports", variables);

     auto block =  gr::digitizers::signal_averager::make(n_ports, window_size);

     return block;
  }
};

struct StftAlgorithmsMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == stft_algorithms_key);
     auto samp_rate    = info.eval_param_value<double>("samp_rate", variables);
     auto delta_t    = info.eval_param_value<double>("delta_t", variables);
     auto alg_id    = static_cast<gr::digitizers::stft_algorithm_id_t>(info.eval_param_value<int>("alg_id", variables));// static_cast, since eval_param_value cannot handle enums :(
     auto win_size    = info.eval_param_value<int>("win_size", variables);
     auto win_type    = info.eval_param_enum("win_type");
     auto fq_low    = info.eval_param_value<double>("fq_low", variables);
     auto fq_hi    = info.eval_param_value<double>("fq_hi", variables);
     auto nbins    = info.eval_param_value<int>("nbins", variables);


     auto block =  gr::digitizers::stft_algorithms::make(samp_rate, delta_t, win_size, win_type, alg_id, fq_low, fq_hi, nbins);

     return block;
  }
};

struct StftGoertzlDynamicMaker : BlockMaker
{
  gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
  {
     assert(info.key == stft_goertzl_dynamic_key);

     auto samp_rate    = info.eval_param_value<double>("samp_rate", variables);
     auto delta_t    = info.eval_param_value<double>("delta_t", variables);
     auto win_size    = info.eval_param_value<int>("win_size", variables);
     auto nbins       = info.eval_param_value<int>("nbins", variables);
     auto bound_decim    = info.eval_param_value<int>("bound_decim", variables);

     auto block =  gr::digitizers::stft_goertzl_dynamic_decimated::make(samp_rate, delta_t, win_size, nbins, bound_decim);

     return block;
  }
};

struct TimeDomainSinkMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == time_domain_sink_key);

        auto signal_name  = info.param_value("signal_name");
        auto signal_unit  = info.param_value("signal_unit");
        auto samp_rate    = info.eval_param_value<double>("samp_rate", variables);
        auto buffer_size  = info.eval_param_value<size_t>("buffer_size", variables);
        auto nr_buffers  = info.eval_param_value<size_t>("nr_buffers", variables);
        auto mode         = info.param_value<int>("acquisition_type");

        auto sink =  gr::digitizers::time_domain_sink::make(signal_name, signal_unit,
              samp_rate, buffer_size, nr_buffers, static_cast<gr::digitizers::time_sink_mode_t>(mode));

        return sink;
    }
};

struct TimeRealignmentMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == time_realignment_key);

        auto samp_rate  = info.eval_param_value<float>("samp_rate", variables);
        auto user_delay = info.eval_param_value<float>("user_delay", variables);

        auto block =  gr::digitizers::time_realignment_ff::make(samp_rate, user_delay);
        return block;
    }
};

struct WrReceiverMaker : BlockMaker
{
    gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == wr_receiver_f_key);
        return gr::digitizers::wr_receiver_f::make();
    }
};


BlockFactory::BlockFactory()
{
  handlers_b["blocks_null_sink"] = boost::shared_ptr<BlockMaker>(new NullSinkMaker());
  handlers_b["blocks_null_source"] = boost::shared_ptr<BlockMaker>(new NullSourceMaker());
  handlers_b["blocks_uchar_to_float"] = boost::shared_ptr<BlockMaker>(new UcharToFloatMaker());
  handlers_b["blocks_vector_to_stream"] = boost::shared_ptr<BlockMaker>(new VectorToStreamMaker());
  handlers_b["blocks_stream_to_vector"] = boost::shared_ptr<BlockMaker>(new StreamToVectorMaker());
  handlers_b["blocks_vector_to_streams"] = boost::shared_ptr<BlockMaker>(new VectorToStreamsMaker());
  handlers_b["analog_sig_source_x"] = boost::shared_ptr<BlockMaker>(new SigSourceMaker());
  handlers_b[blocks_throttle_key] = boost::shared_ptr<BlockMaker>(new ThrottleMaker());
  handlers_b[blocks_tag_share_key] = boost::shared_ptr<BlockMaker>(new TagShareMaker());
  handlers_b[blocks_tag_debug_key] = boost::shared_ptr<BlockMaker>(new TagDebugMaker());

  handlers_b[block_aggregation_key] = boost::shared_ptr<BlockMaker>(new AggregationMaker());
  handlers_b[block_amplitude_and_phase_key] = boost::shared_ptr<BlockMaker>(new AmplitudeAndPhaseMaker());
  handlers_b[block_complex_to_mag_deg_key] = boost::shared_ptr<BlockMaker>(new ComplexToMagDegMaker());
  handlers_b[block_demux_key] = boost::shared_ptr<BlockMaker>(new DemuxMaker());
  handlers_b[block_scaling_offset_key] = boost::shared_ptr<BlockMaker>(new ScalingOffsetMaker());
  handlers_b[block_spectral_peaks_key] = boost::shared_ptr<BlockMaker>(new SpectralPeaksMaker());
  handlers_b[cascade_sink_key] = boost::shared_ptr<BlockMaker>(new CascadeSinkMaker());
  handlers_b[chi_square_fit_key] = boost::shared_ptr<BlockMaker>(new ChiSquareFitMaker());
  handlers_b[decimate_and_adjust_timebase_key] = boost::shared_ptr<BlockMaker>(new DecimateAndAdjustTimebaseMaker());
  handlers_b[edge_trigger_ff_key] = boost::shared_ptr<BlockMaker>(new EdgeTriggerMaker());
  handlers_b[edge_trigger_receiver_f_key] = boost::shared_ptr<BlockMaker>(new EdgeTriggerReceiverMaker());
  handlers_b[demux_ff_key] = boost::shared_ptr<BlockMaker>(new ExtractorMaker());
  handlers_b[freq_sink_f_key] = boost::shared_ptr<BlockMaker>(new FreqSinkMaker());
  handlers_b[function_ff_key] = boost::shared_ptr<BlockMaker>(new FunctionMaker());
  handlers_b[interlock_generation_ff_key] = boost::shared_ptr<BlockMaker>(new InterlockGenerationMaker());
  handlers_b[picoscope_3000a_key] = boost::shared_ptr<BlockMaker>(new Ps3000aMaker());
  handlers_b[picoscope_4000a_key] = boost::shared_ptr<BlockMaker>(new Ps4000aMaker());
  handlers_b[picoscope_6000_key] = boost::shared_ptr<BlockMaker>(new Ps6000Maker());
  handlers_b[post_mortem_sink_key] = boost::shared_ptr<BlockMaker>(new PostMortemSinkMaker());
  handlers_b[signal_averager_key] = boost::shared_ptr<BlockMaker>(new SignalAveragerMaker());
  handlers_b[stft_algorithms_key] = boost::shared_ptr<BlockMaker>(new StftAlgorithmsMaker());
  handlers_b[stft_goertzl_dynamic_key] = boost::shared_ptr<BlockMaker>(new StftGoertzlDynamicMaker());
  handlers_b[time_domain_sink_key] = boost::shared_ptr<BlockMaker>(new TimeDomainSinkMaker());
  handlers_b[time_realignment_key] = boost::shared_ptr<BlockMaker>(new TimeRealignmentMaker());
  handlers_b[wr_receiver_f_key] = boost::shared_ptr<BlockMaker>(new WrReceiverMaker());
}


/*!
 * Affinity is not parsed as a vector for now...
 */
void BlockFactory::common_settings(gr::basic_block_sptr block,
        const BlockInfo &info, const std::vector<BlockInfo> &variables)
{
    if (info.is_param_set("affinity")) {
        auto affinity = info.param_value<int>("affinity");
        block->set_processor_affinity(std::vector<int> {affinity});
    }

    if (info.is_param_set("minoutbuf")) {
        auto minoutbuf = info.eval_param_value<int>("minoutbuf", variables);
        if (minoutbuf > 0) {
          gr::block_sptr blk_ptr = boost::dynamic_pointer_cast<gr::block>(block);
          gr::hier_block2_sptr hb2_ptr = boost::dynamic_pointer_cast<gr::hier_block2>(block);
          if(blk_ptr) {
            blk_ptr->set_min_output_buffer(minoutbuf);
          }
          else if (hb2_ptr) {
            hb2_ptr->set_min_output_buffer(minoutbuf);
          }
          else {
            std::cerr << "cannot set minoutbuf parameter!\n";
          }
        }
    }

    if (info.is_param_set("maxoutbuf")) {
        auto maxoutbuf = info.eval_param_value<int>("maxoutbuf", variables);
        if (maxoutbuf > 0) {
            gr::block_sptr blk_ptr = boost::dynamic_pointer_cast<gr::block>(block);
            gr::hier_block2_sptr hb2_ptr = boost::dynamic_pointer_cast<gr::hier_block2>(block);
            if(blk_ptr) {
              blk_ptr->set_max_output_buffer(maxoutbuf);
            }
            else if (hb2_ptr) {
              hb2_ptr->set_max_output_buffer(maxoutbuf);
            }
            else {
              std::cerr << "cannot set maxoutbuf parameter!\n";
            }
        }
    }
}

gr::basic_block_sptr BlockFactory::make_block(const BlockInfo &info, const std::vector<BlockInfo> &variables)
{
    auto it = handlers_b.find(info.key);

    if (it == handlers_b.end()) {
        std::ostringstream message;
        message << "Exception in " << __FILE__ << ":" << __LINE__ << ": block type " << info.key << " not supported.";
        throw std::invalid_argument(message.str());
    }

    boost::shared_ptr<BlockMaker> maker = it->second;
    auto block = maker->make(info, variables);

    // apply common settings
    common_settings(block, info, variables);

    return block;
}

std::unique_ptr<FlowGraph> make_flowgraph(std::istream &input, const std::map<std::string, std::string> &hw_mapping)
{
	// parse input and replace variables
	flowgraph::GrcParser parser(input);
	parser.parse();
	parser.collapse_variables();

	// obtain title if provided
	std::string title = parser.top_block().param_value("title");
	if (!title.length())
	{
		title = "My Flowgraph";
	}

	// make graph, add blocks and connections
	BlockFactory factory;

	std::unique_ptr<FlowGraph> graph(new FlowGraph(title));

	auto variables = parser.variables();
	std::vector<std::string> disabled_blocks;
 	for (auto info : parser.blocks())
 	{
 	    if ( info.key == "note" ) {
            continue; // skip all "notes" (notes are comments in the *.grc file)
 	    }
 	    if (!info.param_value<bool>("_enabled")) {
 	        disabled_blocks.push_back(info.id);
 	        continue;
 	    }

 		if (hw_mapping.count(info.id)) {
 			info.params["serial_number"] = hw_mapping.find(info.id)->second;
 		}
		auto block = factory.make_block(info, variables);
		graph->add(block, info.id, info.key);
	}

	for (const auto info : parser.connections()) {
	    // connect only if both ends are enabled
	    if (std::count(disabled_blocks.begin(), disabled_blocks.end(), info.src_id)
	            || std::count(disabled_blocks.begin(), disabled_blocks.end(), info.dst_id)) {
	        continue;
	    }
	    graph->connect(info.src_id, info.src_key,
	                   info.dst_id, info.dst_key);
    }
	return std::move(graph);
}

}


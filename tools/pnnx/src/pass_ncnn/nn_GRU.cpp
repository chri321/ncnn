// Copyright 2021 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "pass_ncnn.h"
#include <string.h>

namespace pnnx {

namespace ncnn {

class nn_GRU : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 3
pnnx.Input              input       0 1 input
nn.GRU                  op_0        1 2 input out out_hidden input_size=%input_size hidden_size=%hidden_size num_layers=1 bias=%bias batch_first=%batch_first bidirectional=%bidirectional @weight_ih_l0 @weight_hh_l0 @bias_ih_l0 @bias_hh_l0 @weight_ih_l0_reverse @weight_hh_l0_reverse @bias_ih_l0_reverse @bias_hh_l0_reverse
pnnx.Output             output      2 0 out out_hidden
)PNNXIR";
    }

    const char* type_str() const
    {
        return "GRU";
    }

    const char* name_str() const
    {
        return "gru";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params, const std::map<std::string, Attribute>& captured_attrs) const
    {
        const bool bidirectional = captured_params.at("bidirectional").b;
        const int num_directions = bidirectional ? 2 : 1;
        const int num_output = captured_params.at("hidden_size").i;
        const int input_size = captured_params.at("input_size").i;

        int weight_data_size = num_directions * num_output * input_size * 3;

        op->params["0"] = num_output;
        op->params["1"] = weight_data_size;
        op->params["2"] = bidirectional ? 2 : 0;

        op->attrs["0"] = Attribute();
        op->attrs["0"].data = {0, 0, 0, 0};

        // RUN-hidden-input_size
        {
            if (bidirectional)
                op->attrs["1"] = captured_attrs.at("op_0.weight_ih_l0") + captured_attrs.at("op_0.weight_ih_l0_reverse");
            else
                op->attrs["1"] = captured_attrs.at("op_0.weight_ih_l0");
        }

        op->attrs["2"] = Attribute();
        op->attrs["2"].data = {0, 0, 0, 0};
        if (captured_params.at("bias").b)
        {
            // reduce bias_ih and bias_hh
            std::vector<float> new_bias;
            {
                auto bias_ih = captured_attrs.at("op_0.bias_ih_l0").get_float32_data();
                auto bias_hh = captured_attrs.at("op_0.bias_hh_l0").get_float32_data();

                new_bias.resize(4 * num_output);
                float* bias = (float*)new_bias.data();
                for (int i = 0; i < 2 * num_output; i++)
                {
                    bias[i] = bias_ih[i] + bias_hh[i];
                }
                memcpy(bias + num_output * 2, (const float*)bias_ih.data() + num_output * 2, num_output * sizeof(float));
                memcpy(bias + num_output * 3, (const float*)bias_hh.data() + num_output * 2, num_output * sizeof(float));
            }

            if (bidirectional)
            {
                std::vector<float> new_bias_reverse;
                {
                    auto bias_ih = captured_attrs.at("op_0.bias_ih_l0_reverse").get_float32_data();
                    auto bias_hh = captured_attrs.at("op_0.bias_hh_l0_reverse").get_float32_data();

                    new_bias_reverse.resize(4 * num_output);
                    float* bias = (float*)new_bias_reverse.data();
                    for (int i = 0; i < 2 * num_output; i++)
                    {
                        bias[i] = bias_ih[i] + bias_hh[i];
                    }
                    memcpy(bias + num_output * 2, (const float*)bias_ih.data() + num_output * 2, num_output * sizeof(float));
                    memcpy(bias + num_output * 3, (const float*)bias_hh.data() + num_output * 2, num_output * sizeof(float));
                }

                op->attrs["3"] = Attribute({4, num_output}, new_bias) + Attribute({4, num_output}, new_bias_reverse);
            }
            else
            {
                op->attrs["3"] = Attribute({4, num_output}, new_bias);
            }
        }
        else
        {
            std::vector<float> bias(4 * num_output, 0.f);

            if (bidirectional)
                op->attrs["3"] = Attribute({4, num_output}, bias) + Attribute({4, num_output}, bias);
            else
                op->attrs["3"] = Attribute({4, num_output}, bias);
        }

        op->attrs["4"] = Attribute();
        op->attrs["4"].data = {0, 0, 0, 0};

        // RUN-hidden-hidden
        {
            if (bidirectional)
                op->attrs["5"] = captured_attrs.at("op_0.weight_hh_l0") + captured_attrs.at("op_0.weight_hh_l0_reverse");
            else
                op->attrs["5"] = captured_attrs.at("op_0.weight_hh_l0");
        }
    }
};

REGISTER_GLOBAL_PNNX_NCNN_GRAPH_REWRITER_PASS(nn_GRU, 20)

class nn_GRU_1 : public nn_GRU
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
4 4
pnnx.Input              input       0 1 input
pnnx.Input              in_hidden   0 1 in_hidden
nn.GRU                  op_0        2 2 input in_hidden out out_hidden input_size=%input_size hidden_size=%hidden_size num_layers=1 bias=%bias batch_first=%batch_first bidirectional=%bidirectional @weight_ih_l0 @weight_hh_l0 @bias_ih_l0 @bias_hh_l0 @weight_ih_l0_reverse @weight_hh_l0_reverse @bias_ih_l0_reverse @bias_hh_l0_reverse
pnnx.Output             output      2 0 out out_hidden
)PNNXIR";
    }
};

REGISTER_GLOBAL_PNNX_NCNN_GRAPH_REWRITER_PASS(nn_GRU_1, 20)

class nn_GRU_2 : public nn_GRU
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
nn.GRU                  op_0        1 1 input out input_size=%input_size hidden_size=%hidden_size num_layers=1 bias=%bias batch_first=%batch_first bidirectional=%bidirectional @weight_ih_l0 @weight_hh_l0 @bias_ih_l0 @bias_hh_l0 @weight_ih_l0_reverse @weight_hh_l0_reverse @bias_ih_l0_reverse @bias_hh_l0_reverse
pnnx.Output             output      1 0 out
)PNNXIR";
    }
};

REGISTER_GLOBAL_PNNX_NCNN_GRAPH_REWRITER_PASS(nn_GRU_2, 20)

class nn_GRU_3 : public nn_GRU
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
4 3
pnnx.Input              input       0 1 input
pnnx.Input              in_hidden   0 1 in_hidden
nn.GRU                  op_0        2 1 input in_hidden out input_size=%input_size hidden_size=%hidden_size num_layers=1 bias=%bias batch_first=%batch_first bidirectional=%bidirectional @weight_ih_l0 @weight_hh_l0 @bias_ih_l0 @bias_hh_l0 @weight_ih_l0_reverse @weight_hh_l0_reverse @bias_ih_l0_reverse @bias_hh_l0_reverse
pnnx.Output             output      1 0 out
)PNNXIR";
    }
};

REGISTER_GLOBAL_PNNX_NCNN_GRAPH_REWRITER_PASS(nn_GRU_3, 20)

} // namespace ncnn

} // namespace pnnx

// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <shader/glsl/code_writer.h>
#include <shader/glsl/params.h>
#include <shader/glsl/translator.h>
#include <util/log.h>

namespace shader::usse::glsl {
USSERecompilerGLSL::USSERecompilerGLSL(ProgramState &program_state, CodeWriter &writer, ShaderVariables &variables, const FeatureState &features)
    : USSERecompiler(program_state.actual_program)
    , writer(writer)
    , variables(variables)
    , cond_stack(0) {
    visitor = std::make_unique<USSETranslatorVisitorGLSL>(*this, program_state, writer, variables, features, cur_instr, false);
}

std::string USSERecompilerGLSL::get_condition(std::uint8_t num, bool do_neg) {
    const ExtPredicate predicator = static_cast<ExtPredicate>(num);

    if (predicator >= ExtPredicate::P0 && predicator <= ExtPredicate::P3) {
        num = static_cast<int>(predicator) - static_cast<int>(ExtPredicate::P0);
    } else if (predicator >= ExtPredicate::NEGP0 && predicator <= ExtPredicate::NEGP1) {
        num = static_cast<int>(predicator) - static_cast<int>(ExtPredicate::NEGP0);
        do_neg = !do_neg;
    }

    if (do_neg) {
        return fmt::format("!p{}", num);
    }

    return fmt::format("p{}", num);
}

void USSERecompilerGLSL::compile_break_node(const usse::USSEBreakNode &node) {
    if (node.get_condition() != 0) {
        writer.add_to_current_body(fmt::format("if ({}) {{", get_condition(node.get_condition(), false)));
        writer.indent_current_body();
    }

    writer.add_to_current_body("break;");

    if (node.get_condition() != 0) {
        writer.dedent_current_body();
        writer.add_to_current_body("}");
    }
}

void USSERecompilerGLSL::compile_continue_node(const usse::USSEContinueNode &node) {
    if (node.get_condition() != 0) {
        writer.add_to_current_body(fmt::format("if ({}) {{", get_condition(node.get_condition(), false)));
        writer.indent_current_body();
    }

    writer.add_to_current_body("\tcontinue;");

    if (node.get_condition() != 0) {
        writer.dedent_current_body();
        writer.add_to_current_body("}");
    }
}

void USSERecompilerGLSL::compile_conditional_node(const usse::USSEConditionalNode &cond) {
    writer.add_to_current_body(fmt::format("if ({}) {{", get_condition(cond.negif_condition(), true)));
    writer.indent_current_body();

    compile_block(*cond.if_block());

    if (cond.else_block()) {
        writer.dedent_current_body();
        writer.add_to_current_body("} else {");
        writer.indent_current_body();

        compile_block(*cond.else_block());
    }

    writer.dedent_current_body();
    writer.add_to_current_body("}");
}

void USSERecompilerGLSL::compile_loop_node(const usse::USSELoopNode &loop) {
    writer.add_to_current_body("while (true) {");
    writer.indent_current_body();

    compile_block(*loop.content_block());

    writer.dedent_current_body();
    writer.add_to_current_body("}");
}

void USSERecompilerGLSL::begin_condition(const int cond) {
    writer.add_to_current_body(fmt::format("if ({}) {{", get_condition(cond, false)));
    writer.indent_current_body();

    cond_stack++;
}

void USSERecompilerGLSL::end_condition() {
    if (cond_stack > 0) {
        writer.dedent_current_body();
        writer.add_to_current_body("}");

        cond_stack--;
    }
}

void USSERecompilerGLSL::compile_code_node(const usse::USSECodeNode &code) {
    USSERecompiler::compile_code_node(code);
    variables.flush();
}

void USSERecompilerGLSL::compile_to_function() {
    if (visitor->is_translating_secondary_program()) {
        writer.set_active_body_type(BODY_TYPE_SECONDARY_PROGRAM);
    } else {
        writer.set_active_body_type(BODY_TYPE_PRIMARY_PROGRAM);
    }

    compile_block(tree_block_node);
    writer.set_active_body_type(BODY_TYPE_MAIN);
}
} // namespace shader::usse::glsl
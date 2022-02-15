#pragma once

#include <mzVkCommon.h>

#include <shaderc/shaderc.hpp>

// Compiles a shader to a SPIR-V binary. Returns the binary as
// a vector of 32-bit words.
std::vector<uint32_t> CompileFile(const std::string&         source_name,
                                  shaderc_shader_kind        kind,
                                  const std::string&         source,
                                  std::string&               err,
                                  shaderc_optimization_level optimize)
{
    shaderc::Compiler       compiler;
    shaderc::CompileOptions options;

    // // Like -DMY_DEFINE=1
    // options.AddMacroDefinition("MY_DEFINE", "1");
    options.SetOptimizationLevel(optimize);
    
    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        err = module.GetErrorMessage();
        return std::vector<uint32_t>();
    }

    return {module.cbegin(), module.cend()};
}

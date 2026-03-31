/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "PipeImage.h"

#include <lsp/io/stream.h>

// Adapter that wraps a PipeImage into an lsp::io::Stream so that
// lsp::Connection can read / write LSP messages over pipes.
class PipeStream : public lsp::io::Stream {
public:
	explicit PipeStream(PipeImage& pipe);

	void read(char* buffer, std::size_t size) override;
	void write(const char* buffer, std::size_t size) override;

private:
	PipeImage& fPipe;
};

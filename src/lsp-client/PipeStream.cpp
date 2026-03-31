/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "PipeStream.h"


PipeStream::PipeStream(PipeImage& pipe)
	:
	fPipe(pipe)
{
}


void
PipeStream::read(char* buffer, std::size_t size)
{
	std::size_t totalRead = 0;
	while (totalRead < size) {
		ssize_t n = fPipe.Read(buffer + totalRead, size - totalRead);
		if (n <= 0)
			throw lsp::io::Error("PipeStream: read failed (EOF or pipe error)");
		totalRead += static_cast<std::size_t>(n);
	}
}


void
PipeStream::write(const char* buffer, std::size_t size)
{
	std::size_t totalWritten = 0;
	while (totalWritten < size) {
		ssize_t n = fPipe.Write(buffer + totalWritten, size - totalWritten);
		if (n <= 0)
			throw lsp::io::Error("PipeStream: write failed (pipe error)");
		totalWritten += static_cast<std::size_t>(n);
	}
}

#ifndef __BRAINFUCKPROGRAM_H__
#define __BRAINFUCKPROGRAM_H__

#include "AsmJit/AsmJit.h"

#include <stack>
#include <string>
#include <iostream>

class BrainfuckProgram {
	public:
		/**
		 * Create a brainfuck program using given source code
		 */
		BrainfuckProgram(const std::string &source);

		/**
		 * Compile the program into native code.
		 * Returns true for success and false for failure
		 *
		 * Possible failure cases include mismatching [] and failure from the JIT engine
		 */
		bool compile();

		/**
		 * Execute the program given input. Program's output will be written into output
		 */
		void execute(std::istream &input, std::ostream &output) const;

	protected:
		class RedirectIO {
			public:
				RedirectIO(std::istream &input, std::ostream &output);

				void putchar(char c);

				char getchar();

			protected:
				std::istream &input;
				std::ostream &output;
		};

		typedef uint8_t* (*BrainfuckCompiledProgram)(uint8_t* , const RedirectIO &);

		void optimizeSequence(std::string::iterator &start, char increase, char decrease, AsmJit::Operand &operand);

		void emitPutchar(const AsmJit::Mem &io, const AsmJit::Mem &memory);

		void emitGetchar(const AsmJit::Mem &io, const AsmJit::Mem &memory);

		std::string source;
		AsmJit::Compiler compiler;
		BrainfuckCompiledProgram program;
		std::stack<std::pair<AsmJit::Label, AsmJit::Label > > labelStack;
};

#endif

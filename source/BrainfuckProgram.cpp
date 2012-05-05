#include "BrainfuckProgram.h"

BrainfuckProgram::BrainfuckProgram(const std::string &source):
	source(source) {
	program = NULL;
}

bool BrainfuckProgram::compile() {
	// Reduce code noise
	using namespace AsmJit;

	// Prepare a function to compile to
	// RedirectIO is a pointer here because that is how references are passed.
	EFunction* function = compiler.newFunction(CALL_CONV_DEFAULT, FunctionBuilder2<uint8_t* , uint8_t*, RedirectIO *>());

	// Eliminate calling convention code if possible
	function->setHint(FUNCTION_HINT_NAKED, true);

	// Get the first param as both address and memory location
	GPVar address = compiler.argGP(0);
	Mem memory = byte_ptr(address);

	// Get the RedirectIO structure
	Mem io = byte_ptr(compiler.argGP(1));

	// Start parsing the program
	std::string::iterator opcode = source.begin();

	// Parse each opcode to emit code
	while(opcode != source.end()) {
		switch(*opcode++) {
			// + and - increase and decrease the value at the memory location
			case '+':
			case '-':
					optimizeSequence(--opcode, '+', '-', memory);
				break;
			// > and < increase and decrease the address
			case '>':
			case '<':
					optimizeSequence(--opcode, '>', '<', address);
				break;
			case '[':
					// Push a new context into the label stack
					labelStack.push(std::make_pair(compiler.newLabel(), compiler.newLabel()));

					// Bind the start of the loop to here
					compiler.bind(labelStack.top().first);
					// If the value at the memory loaction is zero
					compiler.cmp(memory, 0);
					// Jump to the second label (end of the loop)
					compiler.jz(labelStack.top().second);
				break;
			case ']':
					// Make sure label stack has a context
					// If it's empty, it means the code is malformed
					if(labelStack.empty())
						return false;

					// Jump to the start of the loop and perform the check again
					compiler.jmp(labelStack.top().first);
					// Bind the end of the loop to here
					compiler.bind(labelStack.top().second);
					// Pop the context off the label stack
					labelStack.pop();
				break;
			case '.':
					emitPutchar(io, memory);
				break;
			case ',':
					emitGetchar(io, memory);
				break;
		}
	}

	// Stack has to be empty when we finish
	if(!labelStack.empty())
		return false;

	// Return the final "tape pointer"
	compiler.ret(address);

	// End the compiled function
	compiler.endFunction();

	// Get the compiled function from the compiler
	program = function_cast<BrainfuckCompiledProgram>(compiler.make());

	// Success if program isn't null
	return program != NULL;
}

void BrainfuckProgram::execute(std::istream &input, std::ostream &output) const {
	// Program isn't compiled yet
	if(program == NULL)
		return;

	// Create memory for the program
	// TODO: Adjustible memory amount
	uint8_t* memory = new uint8_t[0x1000];
	std::fill(memory, memory + 0x1000, 0);

	// Call the program
	program(memory, RedirectIO(input, output));

	// Release memory associated with the program
	delete[] memory;
}

BrainfuckProgram::RedirectIO::RedirectIO(std::istream &input, std::ostream &output):
	input(input), output(output) {

}

void BrainfuckProgram::RedirectIO::putchar(char c) {
	output.put(c);
}

char BrainfuckProgram::RedirectIO::getchar() {
	if(input.eof())
		return 0;

	return input.get();
}

void BrainfuckProgram::optimizeSequence(std::string::iterator &start, char increase, char decrease, AsmJit::Operand &operand) {
	sysint_t count = 0;
	while(*start == increase || *start == decrease) {
		if(*start == increase)
			count += 1;
		if(*start == decrease)
			count -= 1;

		// Advance the iterator for the next opcode
		start += 1;
	}

	// Don't omit code to do nothing
	if(count == 0)
		return;

	// Default to addition
	uint32_t opcode = AsmJit::INST_ADD;

	// If we need to decrease, negate the value and set the opcode to subtract
	if(count < 0) {
		count = -count;
		opcode = AsmJit::INST_SUB;
	}

	// Emit the instruction
	compiler.emit(opcode, operand, AsmJit::imm(count));
}

void BrainfuckProgram::emitPutchar(const AsmJit::Mem &io, const AsmJit::Mem &memory) {
	// Take a byte off the memory stream
	AsmJit::GPVar value = compiler.newGP();
	compiler.movzx(value, memory);

	// Calculate the address for the struct
	AsmJit::GPVar redirectStruct = compiler.newGP();
	compiler.lea(redirectStruct, io);

	// Call io.putchar(value)
	AsmJit::ECall* ctx = compiler.call((reinterpret_cast<void* >(&RedirectIO::putchar)));
	ctx->setPrototype(AsmJit::CALL_CONV_DEFAULT, AsmJit::FunctionBuilder2<void, RedirectIO*, int >());
	ctx->setArgument(0, redirectStruct);
	ctx->setArgument(1, value);
}

void BrainfuckProgram::emitGetchar(const AsmJit::Mem &io, const AsmJit::Mem &memory) {
	// Calculate the address for the struct
	AsmJit::GPVar redirectStruct = compiler.newGP();
	compiler.lea(redirectStruct, io);

	// Function call result
	AsmJit::GPVar result = compiler.newGP();

	// Call io.getchar(value)
	AsmJit::ECall* ctx = compiler.call((reinterpret_cast<void* >(&RedirectIO::getchar)));
	ctx->setPrototype(AsmJit::CALL_CONV_DEFAULT, AsmJit::FunctionBuilder1<int, RedirectIO* >());
	ctx->setArgument(0, redirectStruct);
	ctx->setReturn(result);

	// Move the result into memory location
	compiler.mov(memory, result);
}

#include "MeshConverter.h"

int main(int argc, char** argv)
{

	std::chrono::time_point begin = std::chrono::steady_clock::now();

	if (argc <= 2)
	{
		printf("Convert any 3D Mesh Format to .sbox format\n");
		printf("Insufficient arguments, no file specified!!\n");
		exit(EXIT_FAILURE);
	}

	const char* filename = argv[1];
	const char* exportPath = argv[2];
	MeshConverter::Convert(filename, exportPath);

	std::chrono::time_point end = std::chrono::steady_clock::now();
	uint64_t elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	std::cout << "Total time taken: " << elapsedTime * 1e-3f << std::endl;
	return 0;
}
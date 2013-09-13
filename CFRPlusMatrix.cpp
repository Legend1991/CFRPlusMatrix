#include <limits>
#include <chrono>
#include <random>
#include <vector>
#include "cmdline.h"

using namespace std;
using namespace std::chrono;

class MatrixGame
{
public:
	MatrixGame(int size, mt19937 &rng)
	{
		this->size = size;
		iterationCount = 0;
		payoffs.resize(size * size);

		for (int p = 0; p < 2; p++)
		{
			strategy[p].resize(size);
			cfr[p].resize(size);
		}

		CreateRandom(rng);
	}

	void CreateRandom(mt19937 &rng)
	{
		uniform_real_distribution<double> double_rnd(-1.0, 1.0);

		for (int a = 0; a < size; a++)
		{
			for (int b = 0; b < size; b++)
			{
				payoffs[a * size + b] = double_rnd(rng);
			}
		}
	}

	int GetIterationCount() const { return iterationCount; }
	double GetExploitability() const { return (BestResponse(0) + BestResponse(1)) / 2; }

	void FictitiousPlay()
	{
		iterationCount++;
		FictitiousPlay(0);
		FictitiousPlay(1);
	}

	void CFR()
	{
		iterationCount++;
		CFR(0);
		CFR(1);
	}

	void CFRPlus()
	{
		iterationCount++;
		CFRPlus(0);
		CFRPlus(1);
	}

	void Iteration(int algorithm)
	{
		switch (algorithm)
		{
		case 0: FictitiousPlay(); break;
		case 1: CFR(); break;
		default: CFRPlus(); break;
		}
	}

private:
	double GetPayoff(int p, int a, int b) const
	{ 
		if (p == 0)
			return payoffs[a * size + b]; 
		else
			return -payoffs[b * size + a]; 
	}

	vector<double> GetNormalizedStrategy(int player) const
	{
		vector<double> ns(size);
		double sum = 0;

		for (int i = 0; i < size; i++)
			sum += strategy[player][i];

		if (sum > 0)
		{
			for (int i = 0; i < size; i++)
				ns[i] = strategy[player][i] / sum;
		}
		else
		{
			for (int i = 0; i < size; i++)
				ns[i] = 1.0 / size;
		}

		return ns;
	}

	vector<double> GetCurrentStrategy(int player) const
	{
		vector<double> cs(size);
		double sum = 0;

		for (int i = 0; i < size; i++)
			sum += max(0.0, cfr[player][i]);

		if (sum > 0)
		{
			for (int i = 0; i < size; i++)
				cs[i] = cfr[player][i] > 0 ? cfr[player][i] / sum : 0.0;
		}
		else
		{
			for (int i = 0; i < size; i++)
				cs[i] = 1.0 / size;
		}

		return cs;
	}

	double BestResponse(int player) const
	{
		double maxSum = -numeric_limits<double>::max();

		auto ns = GetNormalizedStrategy(player ^ 1);

		for (int a = 0; a < size; a++)
		{
			double sum = 0;

			for (int b = 0; b < size; b++)
				sum += ns[b] * GetPayoff(player, a, b);

			if (sum > maxSum)
				maxSum = sum;
		}

		return maxSum;
	}

	void FictitiousPlay(int player)
	{
		double maxSum = -numeric_limits<double>::max();
		int bestAction = -1;

		auto ns = GetNormalizedStrategy(player ^ 1);

		for (int a = 0; a < size; a++)
		{
			double sum = 0;

			for (int b = 0; b < size; b++)
				sum += ns[b] * GetPayoff(player, a, b);

			if (sum > maxSum)
			{
				maxSum = sum;
				bestAction = a;
			}
		}

		strategy[player][bestAction]++;

	}

	void CFR(int player)
	{
		vector<double> cfu(size);
		double ev = 0;

		auto sp = GetCurrentStrategy(player);
		auto so = GetCurrentStrategy(player ^ 1);

		for (int a = 0; a < size; a++)
		{
			cfu[a] = 0;

			for (int b = 0; b < size; b++)
				cfu[a] += so[b] * GetPayoff(player, a, b);

			ev += sp[a] * cfu[a];
		}

		for (int a = 0; a < size; a++)
			cfr[player][a] += cfu[a] - ev;

		for (int a = 0; a < size; a++)
			strategy[player][a] += sp[a];

	}

	void CFRPlus(int player)
	{
		vector<double> cfu(size);
		double ev = 0;

		auto sp = GetCurrentStrategy(player);
		auto so = GetCurrentStrategy(player ^ 1);

		for (int a = 0; a < size; a++)
		{
			cfu[a] = 0;

			for (int b = 0; b < size; b++)
				cfu[a] += so[b] * GetPayoff(player, a, b);

			ev += sp[a] * cfu[a];
		}

		for (int a = 0; a < size; a++)
			cfr[player][a] = std::max(0.0, cfr[player][a] + cfu[a] - ev);

		for (int a = 0; a < size; a++)
			strategy[player][a] += sp[a] * iterationCount * iterationCount;

	}


private:
	int size;
	int iterationCount;
	vector<double> payoffs;
	vector<double> strategy[2];
	vector<double> cfr[2];

};

int Run(int algorithm, int size, double epsilon, mt19937 &rng)
{
	MatrixGame m(size, rng);
	double e;
	do
	{
		m.Iteration(algorithm);
		e = m.GetExploitability();
	} while (e > epsilon);

	return m.GetIterationCount();
}

void RunMany(int n, int algorithm, int size, double epsilon)
{
	random_device rd;
	mt19937 rng;
	rng.seed(rd());

	double sum = 0;
	int min = numeric_limits<int>::max();
	int max = numeric_limits<int>::min();

	for (int i = 0; i < n; i++)
	{
		printf("\r%d/%d", i + 1, n);
		fflush(stdout);
		auto nit = Run(algorithm, size, epsilon, rng);
		min = std::min(min, nit);
		max = std::max(max, nit);
		sum += nit;
	}

	printf("\rmin %d | max %d | avg %.1f\n", min, max, sum / n);

}

char const *algorithmNames[] = { "Fictitious play", "CFR", "CFR+" };

int main(int argc, char *argv[])
{
	CommandLine::Integer algorithm("a", false, "Algorithm (0 = Fictitious play, 1 = CFR, 2 = CFR+)", 0, 2, 2);
	CommandLine::Integer size("s", false, "Matrix size", 2, 100000, 1000);
	CommandLine::Real epsilon("e", false, "Epsilon", 0.000000000001, 1, 0.0001);
	CommandLine::Integer nruns("n", false, "Number of times to run", 1, 100000, 1);
	CommandLine::Parser::Parse(argc, argv);

	printf("Algorithm: %s\n", algorithmNames[algorithm]);
	printf("Matrix size: %d\n", (int)size);
	printf("Epsilon: %f\n", (double)epsilon);
	printf("N: %d\n", (int)nruns);

	if (nruns > 1)
	{
		RunMany(nruns, algorithm, size, epsilon);
		return 0;
	}

	printf("init\n");

	random_device rd;
	mt19937 rng;
	rng.seed(rd());

	MatrixGame m(size, rng);
	double e;

	printf("start\n");

	high_resolution_clock clock;
	auto startTime = clock.now();

	do
	{
		m.Iteration(algorithm);

		e = m.GetExploitability();

		auto t = duration_cast<milliseconds>(clock.now() - startTime).count();

		printf("i=%d t=%.2f e=%.6f\n", m.GetIterationCount(), t / 1000.0, e);
	} while (e > epsilon);

	return 0;
}


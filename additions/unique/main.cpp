#include <stdio.h>
#include <sstream>
#include <vector>

class polygon_num {
	friend class Configuration;
public:
	polygon_num() {
		internal = external = NULL;
		i_max = e_max = 0;
	}
	~polygon_num() {
		delete []internal;
		delete []external;
	}

	bool operator == (const polygon_num &other) const {
		if (i_max != other.i_max)
			return false;

		if (e_max != other.e_max)
			return false;

		for (int i = 0; i <= i_max; ++i) {
			if (internal[i] != other.internal[i])
				return false;
		}

		for (int i = 0; i <= e_max; ++i) {
			if (external[i] != other.external[i])
				return false;
		}
		return true;
	}

protected:
	int *internal, *external;
	int i_max, e_max;
};

class Omatrix {
	friend class Configuration;

public:
	Omatrix() {
		vals = NULL;
		parallels = NULL;
		positions = NULL;
	}
	~Omatrix() {
		if (vals) {
			for (int i = 0; i < N; ++i)
				delete []vals[i];
		}
		delete []vals;
		delete []parallels;
		delete []positions;
	}

	void set_N(int N_) {
		if (vals) {
			for (int i = 0; i < N; ++i)
				delete []vals[i];
		}
		delete []vals;
		N = N_;
		N2 = 2*N;
		vals = new int*[N];
		for (int i = 0; i < N; ++i) {
			vals[i] = new int[N - 1];
		}

		if (parallels)
			delete []parallels;
		parallels = new int[N];

		if (positions)
			delete []positions;
		positions = new int[N];
	}

	int get_N() const {
		return N;
	}

	int norm(int v) const {
		return (v + N2) % N;
	}

	int norm2(int v) const {
		return (v + N2) % N2;
	}

	int get_val(int rotation, int i, int j) const {
		int val;
		if (norm2(i + rotation) < N) {
			val = vals[norm2(i + rotation)][j];
		} else {
			int line = norm(i + rotation);
			line = parallels[line];
			val = vals[line][positions[line] - j - 1];
		}
		if ((rotation <= N && val >= rotation) || (rotation > N && val <= norm(rotation-1))) {
			return norm(val - rotation);
		}
		val = parallels[val];
		return norm(val - rotation);
	}

	int get_line_len(int l, int rotation) const {
		return positions[norm(l + rotation)];
	}

	void get_reflected(Omatrix &o) const {
		o.set_N(N);
		int d = 0;
		if (parallels[0] != 0)
			d = 1;
		o.positions[0] = positions[convert_n2o(0, d)];
		if (d == 1)
			o.positions[1] = positions[convert_n2o(1, d)];

		for (int j = 0; j < positions[convert_n2o(0, d)]; ++j) {
			o.vals[0][j] = convert_o2n(vals[convert_n2o(0, d)][j], d);
			if (d == 1) {
				o.vals[1][j] = convert_o2n(vals[convert_n2o(1, d)][j], d);
			}
		}

		for (int i = 1+d; i < N; ++i) {
			int ind = convert_n2o(i, d);
			int len = positions[ind];
			o.positions[i] = len;
			for (int j = 0; j < len; ++j) {
				o.vals[i][j] = convert_o2n(vals[ind][len - j - 1], d);
			}
		}
	}

	bool check_rotation(int rotation) const {
		return parallels[norm(rotation - 1)] <= norm(rotation - 1);
	}

	bool operator == (const Omatrix &o2) const {
		for (int i = 0; i < N2; ++i) {
			if (check_rotation(i)) {
				if (cmp_rotation(o2, i))
					return true;
			}
		}
		Omatrix o1;
		get_reflected(o1);
		for (int i = 0; i < N2; ++i) {
			if (o1.check_rotation(i)) {
				if (o1.cmp_rotation(o2, i))
					return true;
			}
		}
		return false;
	}
protected:
	int convert_n2o(int v, int d) const {
		v = norm2(d - v);
		if (v >= N) {
			return parallels[v - N];
		}
		return v;
	}

	int convert_o2n(int v, int d) const {
		if (v > d) {
			v = parallels[v] + N;
		}
		return norm(d - v);
	}

	bool cmp_rotation(const Omatrix &o2, int rotation) const {
		for (int i = 0; i < N; ++i) {
			if (positions[i] != o2.get_line_len(i, rotation))
				return false;

			for (int j = 0; j < positions[i]; ++j) {
				if (vals[i][j] != o2.get_val(rotation, i, j))
					return false;
			}
		}
		return true;
	}

	int **vals;
	int N, N2;
	int *parallels;
	int *positions;
};

class Configuration {
public:
	Configuration() {
		N = k = gens_n = 0;
		gens = NULL;
	}
	~Configuration() {
		delete []gens;
	}

	void set_gens(const char *str) {
		std::stringstream ss(str);
		std::vector<int> g;
		int gen, max_gen = 0;
		while (!(ss >> gen).fail()) {
			g.push_back(gen);
			if (gen > max_gen)
				max_gen = gen;
		}
		N = max_gen + 2;
		k = N * (N - 1) / 2 - g.size();

		gens_n = g.size();
		gens = new int[gens_n];
		for (int i = 0; i < gens_n; ++i)
			gens[i] = g[i];

		calc_Omatrix(o);
		calc_polygon_num(p_num);
	}

	int get_N() {
		return N;
	}

	int get_k() {
		return k;
	}

	void print_gens() {
		for (int i = 0; i < gens_n; ++i)
			printf("%d ", gens[i]);
		printf("\n");
	}

	void print_gens(FILE *f) {
		for (int i = 0; i < gens_n; ++i)
			fprintf(f, "%d ", gens[i]);
		fprintf(f, "\n");
	}

	void print() {
		print_gens();

		printf("N = ", N);
		printf("k = ", k);
	}

	bool operator == (const Configuration &other) const {
		return p_num == other.get_polygon_num() && o == other.get_Omatrix();
	}

	const Omatrix &get_Omatrix() const {
		return o;
	}

	const polygon_num &get_polygon_num() const {
		return p_num;
	}

protected:
	void calc_Omatrix(Omatrix &matrix) const {
		matrix.set_N(N);

		int *lines = new int[N];

		for (int i = 0; i < N; ++i) {
			lines[i] = i;
			matrix.positions[i] = 0;
			matrix.parallels[i] = i;
		}

		for (int i = 0; i < gens_n; ++i) {
			int gen = gens[i];
			int first = lines[gen];
			int second = lines[gen + 1];

			matrix.vals[first][matrix.positions[first]] = second;
			lines[gen] = second;

			matrix.vals[second][matrix.positions[second]] = first;
			lines[gen + 1] = first;

			++matrix.positions[first];
			++matrix.positions[second];
		}

		for (int i = 0; i < N - 1; ++i) {
			if (lines[i] < lines[i + 1]) {
				matrix.parallels[lines[i]] = lines[i + 1];
				matrix.parallels[lines[i + 1]] = lines[i];
			}
		}

		delete []lines;
	}

	void calc_polygon_num(polygon_num &pn) const {
		bool *is_int = new bool[N+1];
		int *n = new int[N+1];
		int *s_int = new int[N+1];
		int *s_ext = new int[N+1];

		for (int i = 0; i <= N; ++i) {
			n[i] = 0;
			is_int[i] = false;
			s_int[i] = s_ext[i] = 0;
		}

		pn.i_max = pn.e_max = 0;

		for (int i = 0; i < gens_n; ++i) {
			int gen = gens[i];
			++n[gen];
			++n[gen + 2];

			if (is_int[gen+1]) {
				int c = n[gen+1] + 1;
				if (pn.i_max < c)
					pn.i_max = c;
				++s_int[c];
			} else {
				int c = n[gen+1] + 1;
				if (pn.e_max < c)
					pn.e_max = c;
				++s_ext[c];
			}
			n[gen+1] = 1;
			is_int[gen+1] = true;
		}

		for (int i = 0; i <= N; ++i) {
			int c = n[i];
			if (pn.e_max < c)
				pn.e_max = c;
			++s_ext[c];
		}

		pn.internal = new int[pn.i_max+1];
		pn.external = new int[pn.e_max+1];

		for (int i = 0; i <= pn.i_max; ++i) {
			pn.internal[i] = s_int[i];
		}

		for (int i = 0; i <= pn.e_max; ++i) {
			pn.external[i] = s_ext[i];
		}

		delete []is_int;
		delete []n;
		delete []s_int;
		delete []s_ext;
	}

	Omatrix o;
	polygon_num p_num;

	int N, k;
	int *gens;
	int gens_n;
};

int main(int argc, char **argv) {
	if (argc != 3)
		return 0;
	FILE *fin = fopen(argv[1], "r");
	if (!fin)
		return 0;

	FILE *fout = fopen(argv[2], "w");
	if (!fout)
		return 0;

	char str[2048];
	std::vector<Configuration *> confs;

	while (fgets(str, 1024, fin) > 0) {
		if (!str[0])
			continue;
		Configuration *conf = new Configuration;
		conf->set_gens(str);

		std::vector<Configuration *>::iterator it = confs.begin();

		for (; it != confs.end(); ++it) {
			if (*(*it) == *conf)
				break;
		}

		if (it == confs.end()) {
			confs.push_back(conf);
			conf->print_gens(fout);
		} else {
			delete conf;
		}

		static int cnt = 0;
		if (cnt % 10000 == 0) {
			printf("%d\n", cnt);
		}
		++cnt;
	}
	fclose(fin);
	fclose(fout);
	return 0;
}
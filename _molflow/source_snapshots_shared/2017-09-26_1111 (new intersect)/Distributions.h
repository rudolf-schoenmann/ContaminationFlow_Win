#pragma once




//#include "File.h" //FileReader for LoadCSV
class FileReader;
#include <vector>
#include <algorithm>

template <class Datatype> class Distribution{ //All methods except Interpolate
protected:
	std::vector<std::pair<double,Datatype>> values;
public:
	Distribution(); //Sets loglog to false
	void AddPair(const std::pair<double, Datatype>& pair, const bool& keepOrdered=false);
	void AddPair(const double& x, const Datatype& y, const bool& keepOrdered=false);
	void RemoveValue(const size_t& pos);
	void SetPair(const size_t& index, const std::pair<double, Datatype>& pair);
	void SetPair(const size_t& index, const double& x, const Datatype& y);
	void SetValues(std::vector<std::pair<double, Datatype>> insertValues, const bool& sort); //sort then set
	void SetX(const size_t& index, const double& x);
	void SetY(const size_t& index, const Datatype& y);
	void Resize(const size_t& N); //Clear, resize and shrink.
	size_t GetSize();
	double GetX(const size_t& index);
	Datatype GetY(const size_t& index); //GetYValue seems reserved
	bool isLogLog;
};

template <class Datatype> Distribution<Datatype>::Distribution() {
	//Placeholder to allow simple initialization
	isLogLog = false;
}

template <class Datatype> void Distribution<Datatype>::AddPair(const std::pair<double, Datatype>& pair, const bool& keepOrdered) {
	if (keepOrdered) {
		//Assuming existing values are stored in order
		size_t pos = 0;
		for (; pos<this->values.size() && pair.first>this->values[pos].first; pos++); //find pos to insert
		values.insert(this->values.begin() + pos, pair); //inserts or appends, depending on pos
	}
	else {
		values.push_back(pair);
	}
}

template <class Datatype> void Distribution<Datatype>::AddPair(const double& x, const Datatype& y, const bool& keepOrdered) {
	AddPair(std::make_pair(x, y),keepOrdered);
}

template <class Datatype> void Distribution<Datatype>::RemoveValue(const size_t& pos) {
	values.erase(values.begin() + pos);
}

template <class Datatype> void Distribution<Datatype>::SetPair(const size_t& index, const std::pair<double, Datatype>& pair) {
	_ASSERTE(index < values.size());
	values[index]=pair;
}

template <class Datatype> void Distribution<Datatype>::SetPair(const size_t& index, const double& x, const Datatype& y) {
	SetPair(index,std::make_pair(x, y));
}


template <class Datatype> void Distribution<Datatype>::SetX(const size_t& index, const double& x) {
	_ASSERTE(index < values.size());
	values[index].first = x;
}

template <class Datatype> void Distribution<Datatype>::SetY(const size_t& index, const Datatype& x) {
	_ASSERTE(index < values.size());
	values[index].second = y;
}

template <class Datatype> void Distribution<Datatype>::Resize(const size_t& N) {
	std::vector<std::pair<double, Datatype>> vecs;
	vecs.resize(N);
	values.swap(vecs);
}

template <class Datatype> size_t Distribution<Datatype>::GetSize() {
	return values.size();
}

template <class Datatype> double Distribution<Datatype>::GetX(const size_t& index) {
	_ASSERTE(index < values.size());
	return values[index].first;
}

template <class Datatype> Datatype Distribution<Datatype>::GetY(const size_t& index) {
	_ASSERTE(index < values.size());
	return values[index].second;
}

class Distribution2D:public Distribution<double> { //Standard x-y pairs of double
public:
	double InterpolateY(const double &x,const bool& allowExtrapolate) const; //interpolates the Y value corresponding to X (allows extrapolation)
	double InterpolateX(const double &y,const bool& allowExtrapolate) const; //interpolates the X value corresponding to Y (allows extrapolation)
};

class DistributionND:public Distribution<std::vector<double>> { //x-y pairs where y is a vector of double values
public:
	std::vector<double> InterpolateY(const double& x, const bool& allowExtrapolate);
	double InterpolateX(const double& y, const size_t& elementIndex, const bool& allowExtrapolate);
};

template <class Datatype> bool sorter (const std::pair<double, Datatype> &i, const std::pair<double, Datatype> &j) {
	return (i.first<j.first);
}

template <class Datatype> void Distribution<Datatype>::SetValues(std::vector<std::pair<double, Datatype>> insertValues, const bool& sort) { //sort then set
	if (sort) std::sort(insertValues.begin(), insertValues.end()/*, sorter*/); //sort pairs by time first
	this->values = insertValues;
}
#ifndef SPLITTERCELL_DISTRIBUTION_H
#define SPLITTERCELL_DISTRIBUTION_H

#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <utility>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <boost/dynamic_bitset.hpp>
#include <sstream>
#include <memory>
#include <future>
#include <algorithm>

namespace splittercell {
    class Flock {
    public:
        /* Constructors */
        Flock(const std::vector<unsigned int> &args, const std::vector<unsigned int> &cond = {}, const std::vector<double> &distribution = {});
        /* Accessors */
        unsigned int size() const {return _size;}
        const std::vector<double> &distribution() const {return _distribution;}
        void set_probabilities(const std::vector<double> &probabilities) {_distribution = probabilities;}
        const std::vector<unsigned int> &conditioned() const {return _conditioned;}
        const std::vector<unsigned int> &conditioning() const {return _conditioning;}
        /* Modifiers*/
        void refine(unsigned int argument, bool positive, double coefficient);
        std::shared_ptr<Flock> marginalize(const std::vector<unsigned int> &args_to_keep) const;
        void marginalize_self(const std::vector<unsigned int> &args_to_keep);
        std::shared_ptr<Flock> combine(const std::shared_ptr<Flock> &flock) const;

        std::string to_str() const;
        bool operator==(const Flock &other) const {return (_conditioned == other._conditioned) &&
                    (_conditioning == other._conditioning) && (_distribution == other._distribution); }

    private:
        std::vector<unsigned int> _conditioned, _conditioning;
        std::vector<double> _distribution;
        std::unordered_map<unsigned int, unsigned int> _mapping;
        unsigned int _size;

        void map_arguments();
        std::vector<double> marginalized_distribution(const std::vector<unsigned int> &args_to_keep) const;
    };

    /********************** Implementation ************************/

    Flock::Flock(const std::vector<unsigned int> &args, const std::vector<unsigned int> &cond, const std::vector<double> &distribution) :
            _conditioned(args), _conditioning(cond), _distribution(distribution), _size(args.size() + cond.size()) {
        unsigned int limit = std::numeric_limits<unsigned int>::digits;
        if(_size > (limit - 2))
            throw std::overflow_error("Too many arguments in the flock.");
        unsigned int num_of_models = (unsigned int)(1 << _size);
        double initial_belief      = 1.0 / (1 << _size);
        if(_distribution.empty())
            _distribution          = std::vector<double>(num_of_models, initial_belief);

        map_arguments();
    }

    std::string Flock::to_str() const {
        std::stringstream ss;
        for (double val : _distribution) ss << val << " ";
        std::string s = ss.str();
        s.pop_back();
        return s;
    }

    void Flock::refine(unsigned int argument, bool positive, double coefficient) {
        unsigned int index = _mapping[argument];
        if(index >= _conditioned.size())
            throw std::invalid_argument("Only conditioned arguments can be refined.");
        for (unsigned int i = 0; i < _distribution.size(); i++) {
            boost::dynamic_bitset<> binary(_size, i); //Current model in binary notation
            if (binary.test(index) == positive) { //If the model satisfies argument+side of update
                unsigned int opposite = (unsigned int) binary.flip(index).to_ulong(); //Closest model not satisfying
                double opposite_val = _distribution[opposite];
                _distribution[opposite] *= (1 - coefficient);
                _distribution[i] += coefficient * opposite_val;
            }
        }
    }

    std::vector<double> Flock::marginalized_distribution(const std::vector<unsigned int> &args_to_keep) const {
        /* New mapping creation (because marginalization put holes in the previous one) */
        unsigned int index = 0;
        std::map<unsigned int, unsigned int> mapping;
        if(args_to_keep == _conditioned)
            return _distribution;
        for(unsigned int arg : args_to_keep)
            if(_mapping.find(arg) != _mapping.end())
                mapping[_mapping.at(arg)] = index++;
        for(unsigned int arg : _conditioning)
            if(_mapping.find(arg) != _mapping.end() && mapping.find(_mapping.at(arg)) == mapping.cend())
                mapping[_mapping.at(arg)] = index++;

        /* Actual marginalization */
        unsigned int marginalized_size = (unsigned int)(1 << mapping.size());
        auto distribution = std::vector<double>(marginalized_size, 0.0);
        for(unsigned int i = 0; i < _distribution.size(); i++) {
            boost::dynamic_bitset<> start(_size, i), end(mapping.size(), 0);
            for(auto map : mapping)
                end.set(map.second, start[map.first]);
            distribution[end.to_ulong()] += _distribution[i];
        }
        return distribution;
    }

    std::shared_ptr<Flock> Flock::marginalize(const std::vector<unsigned int> &args_to_keep) const {
        return std::make_shared<Flock>(args_to_keep, _conditioning, marginalized_distribution(args_to_keep));
    }

    void Flock::marginalize_self(const std::vector<unsigned int> &args_to_keep) {
        _conditioned  = args_to_keep;
        _distribution = marginalized_distribution(args_to_keep);
        map_arguments();
    }

    std::shared_ptr<Flock> Flock::combine(const std::shared_ptr<Flock> &flock) const {
        /* Combined flock creation */
        std::vector<unsigned int> conditioned, conditioning;
        conditioned.insert(conditioned.end(), _conditioned.cbegin(), _conditioned.cend());
        conditioned.insert(conditioned.end(), flock->_conditioned.cbegin(), flock->_conditioned.cend());
        /* Remove the conditioning arguments that are conditioned in the other flock (Bayes' rule) */
        std::copy_if(_conditioning.cbegin(), _conditioning.cend(), std::back_inserter(conditioning),
                     [flock](unsigned int i){return std::find(flock->_conditioned.cbegin(), flock->_conditioned.cend(), i) == flock->_conditioned.cend();});
        std::copy_if(flock->_conditioning.cbegin(), flock->_conditioning.cend(), std::back_inserter(conditioning),
                     [this](unsigned int i){return std::find(this->_conditioned.cbegin(), this->_conditioned.cend(), i) == this->_conditioned.cend();});
        auto combinedflock = std::make_shared<Flock>(conditioned, conditioning);
        unsigned int limit = std::numeric_limits<unsigned int>::digits - 2;
        if(combinedflock->size() > limit)
            throw std::overflow_error("Too many arguments in the final combined flock.");

        /* Mapping between combined flock indexes and split flock index */
        std::unordered_map<unsigned int, std::pair<unsigned int, unsigned int>> splitindex;
        conditioned.insert(conditioned.end(), conditioning.cbegin(), conditioning.cend()); //Just to have the whole set of arguments
        for(auto arg : conditioned) {
            auto itindexself = _mapping.find(arg);
            unsigned int indexself = (itindexself == _mapping.end()) ? 0 : itindexself->second + 1; //Careful, mandatory to keep unsigned
            auto itindexother = flock->_mapping.find(arg);
            unsigned int indexother = (itindexother == flock->_mapping.end()) ? 0 : itindexother->second + 1; //Careful
            splitindex.emplace(combinedflock->_mapping[arg], std::make_pair<>(indexself, indexother));
        }

        /* Actual combination */
        for(unsigned int i = 0; i < (unsigned int)(1 << combinedflock->size()); i++) {
            boost::dynamic_bitset<> start(combinedflock->size(), i), end1(_size, 0), end2(flock->size(), 0);
            for(unsigned int j = 0; j < start.size(); j++) {
                auto indexes = splitindex[j];
                if(indexes.first != 0)
                    end1[indexes.first-1] = start[j];
                if(indexes.second != 0)
                    end2[indexes.second-1] = start[j];
            }
            combinedflock->_distribution[i] = _distribution[end1.to_ulong()] * flock->_distribution[end2.to_ulong()];
        }
        return combinedflock;
    }

    /* Mapping argument <-> index to be (somewhat) order agnostic, conditioned first, then conditioning */
    void Flock::map_arguments() {
        unsigned int index = 0;
        for(unsigned int arg : _conditioned)
            _mapping[arg] = index++;
        for(unsigned int arg : _conditioning)
            _mapping[arg] = index++;
    }

    /*************************************************************************************************************************************/

    class Distribution {
    public:
        /* Constructors */
        Distribution(const std::vector<std::shared_ptr<Flock>> &flocks);
        /* Accessors */
        std::unordered_map<unsigned int, double> operator[](const std::vector<unsigned int> &arguments) const;
        void set_probabilities(unsigned int flock, const std::vector<double> &probabilities) {
            _flocks[flock]->set_probabilities(probabilities);
            for(auto arg : _flocks[flock]->conditioned())
                _cache_is_valid[arg] = false;
        }
        std::shared_ptr<Flock> flock(unsigned int flock) const {return _flocks.at(flock);}
        /* Modifiers */
        void refine(unsigned int argument, bool positive, double coefficient) {
            _flocks[_mapping[argument]]->refine(argument, positive, coefficient);
            _cache_is_valid[argument] = false;
        }
        std::shared_ptr<Flock> marginalize(unsigned int flock, const std::vector<unsigned int> &args_to_keep) {
            return _flocks[flock]->marginalize(args_to_keep);
        }

        std::string to_str() const;

    private:
        std::vector<std::shared_ptr<Flock>> _flocks;
        std::unordered_map<unsigned int, unsigned int> _mapping;
        std::unordered_map<unsigned int, double> _belief_cache;
        std::unordered_map<unsigned int, bool> _cache_is_valid;

        void find_conditioning(unsigned int argument, std::set<unsigned int> &conditioning) const;
        std::shared_ptr<Flock> find_and_combine(const std::vector<unsigned int> &arguments) const;
    };

    /********************** Implementation ************************/

    Distribution::Distribution(const std::vector<std::shared_ptr<Flock>> &flocks) : _flocks(flocks) {
        unsigned int flock_index = 0;
        for (auto flock : _flocks) {
            for (auto conditioned : flock->conditioned()) {
                if (_mapping.find(conditioned) != _mapping.end())
                    throw std::invalid_argument("An argument cannot be in different flocks.");
                _mapping[conditioned] = flock_index;
                _belief_cache[conditioned] = 0.5;
                _cache_is_valid[conditioned] = true;
            }
            flock_index++;
        }
    }

    std::unordered_map<unsigned int, double> Distribution::operator[](const std::vector<unsigned int> &arguments) const {
        std::unordered_map<unsigned int, double> beliefs;
        std::shared_ptr<Flock> flock = nullptr;
        for(auto arg : arguments) {
            if(_cache_is_valid.at(arg))
                beliefs[arg] = _belief_cache.at(arg);
            else {
                if(flock == nullptr)
                    flock = find_and_combine(arguments);
                beliefs[arg] = flock->marginalize({arg})->distribution()[1];
            }
        }

        return beliefs;
    }

    void Distribution::find_conditioning(unsigned int argument, std::set<unsigned int> &conditioning) const {
        auto f = _flocks[_mapping.at(argument)];
        for(auto cond : f->conditioning()) {
            conditioning.insert(cond);
            find_conditioning(cond, conditioning);
        }
    }

    std::shared_ptr<Flock> Distribution::find_and_combine(const std::vector<unsigned int> &arguments) const {
        std::set<unsigned int> conditioning_args;
        std::set<std::shared_ptr<Flock>> conditioning_flocks;
        for(auto arg : arguments) {
            find_conditioning(arg, conditioning_args);
            conditioning_flocks.insert(_flocks[_mapping.at(arg)]);
        }
        for(auto arg : conditioning_args)
            conditioning_flocks.insert(_flocks[_mapping.at(arg)]);

        std::deque<std::shared_ptr<Flock>> flock_deque(conditioning_flocks.cbegin(), conditioning_flocks.cend());
        auto combined = flock_deque.front();
        flock_deque.pop_front();
        unsigned int limit = std::numeric_limits<unsigned int>::digits - 2;
        while(!flock_deque.empty()) {
            auto next = flock_deque.front();
            if(combined->size() + next->size() > limit) {
                auto all = std::vector<unsigned int>(arguments);
                all.insert(all.cend(), conditioning_args.cbegin(), conditioning_args.cend());
                combined = combined->marginalize(all);
                next = next->marginalize(all);
            }
            combined = combined->combine(next);
            flock_deque.pop_front();
        }

        return combined->marginalize(arguments);
    }

    std::string Distribution::to_str() const {
        std::stringstream ss;
        for (auto flock : _flocks)
            ss << flock->to_str() << " ";
        std::string s = ss.str();
        s.pop_back();
        return s;
    }
}


#endif //SPLITTERCELL_DISTRIBUTION_H

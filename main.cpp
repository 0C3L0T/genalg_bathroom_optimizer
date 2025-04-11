#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace::std;

const int POPULATION_SIZE = 32;
const int GENERATIONS = 4;
const int N_SURVIVORS = 16;

struct Residence {
    // for now we only focus on bathroom, could be stored more efficiently
    bool has_shower;
    bool has_bath;
    bool has_bath_and_shower;
    bool has_sink;
    bool has_dual_sink;
    bool has_curtain;
    bool has_radiator;
    bool has_closet;
    bool has_wallsocket;
    bool has_thermostatic;
};

string display_residence(Residence* residence) {
    string result;

    if (residence->has_shower)        result += "shower (4), \n";
    if (residence->has_bath)          result += "bath (6), \n";
    if (residence->has_bath_and_shower) result += "bath & shower (7), \n";
    if (residence->has_sink)          result += "sink (0.5), \n";
    if (residence->has_dual_sink)     result += "dual sink (1.0), \n";
    if (residence->has_curtain)       result += "curtain (1.0), \n";
    if (residence->has_radiator)      result += "radiator (1.5), \n";
    if (residence->has_closet)        result += "closet (2.0), \n";
    if (residence->has_wallsocket)    result += "wall socket (0.5), \n";
    if (residence->has_thermostatic)  result += "thermostatic valve (1.0), ";

    // Remove trailing comma and space if not empty
    if (!result.empty()) {
        result.erase(result.size() - 2); // remove last ", "
    }

    return result;
}

struct Individual {
    Residence residence;
    float fitness;
};

string display_individual(Individual* individual) {
    std::ostringstream out;

    out << "Fitness: " << std::fixed << std::setprecision(2) << individual->fitness << "\n";
    out << "Residence features: " << "\n" << display_residence(&individual->residence);

    return out.str();
}

struct BumpAllocator {
    Individual* buffer;
    int capacity;
    int items;

    void init(int population_size) {
        buffer = reinterpret_cast<Individual*>(new char[sizeof(Individual) * population_size]);
        capacity = population_size;
        items = 0;
    };

    Individual* allocate(int nr_individuals) {
        assert(items + nr_individuals <= capacity);

        Individual* ptr = buffer + items * sizeof(Individual);
        items += nr_individuals;
        return ptr;
    };

    Individual* get_individual_by_index(int index) {
        assert(index <= items);

        return buffer + index * sizeof(Individual);
    };

    void reset() {
        items = 0;
    };

    void destroy() {
        delete[] buffer;
    };
};

void generate_random_individual(BumpAllocator* population_allocator) {
    Individual* individual = population_allocator->allocate(1);

    // Reset fitness
    individual->fitness = 0.0f;

    // Randomly pick one of: shower, bath, bath_and_shower
    int bathing_type = rand() % 3;
    Residence& res = individual->residence;

    res.has_shower = (bathing_type == 0);
    res.has_bath = (bathing_type == 1);
    res.has_bath_and_shower = (bathing_type == 2);

    // Randomize other features
    res.has_sink = rand() % 2;
    res.has_dual_sink = rand() % 2;
    res.has_curtain = rand() % 2;
    res.has_radiator = rand() % 2;
    res.has_closet = rand() % 2;
    res.has_wallsocket = rand() % 2;
    res.has_thermostatic = rand() % 2;
}

void generate_random_individuals(BumpAllocator *population_allocator, int nr_individuals) {
    for (int i = 0; i < nr_individuals; i++) {
        generate_random_individual(population_allocator);
    }
};

float calculate_residence_points(Residence residence) {
    float bath_shower_points = 0;
    if (residence.has_bath_and_shower) {
        bath_shower_points = 7;
    } else if (residence.has_bath) {
        bath_shower_points = 6;
    } else if (residence.has_shower) {
        bath_shower_points = 4;
    }

    float amenity_points = 0;
    if (residence.has_sink)         amenity_points += 0.5;
    if (residence.has_dual_sink)    amenity_points += 1.0;
    if (residence.has_curtain)      amenity_points += 1.0;
    if (residence.has_radiator)     amenity_points += 1.5;
    if (residence.has_closet)       amenity_points += 2.0;
    if (residence.has_wallsocket)   amenity_points += 0.5;
    if (residence.has_thermostatic) amenity_points += 1.0;

    // limit points to twice the points for bath/shower
    return min(bath_shower_points + amenity_points, bath_shower_points * 2);
}

float calculate_residence_cost(Residence residence) {
    float cost = 0;

    if (residence.has_sink)         cost += 50;
    if (residence.has_dual_sink)    cost += 100;
    if (residence.has_curtain)      cost += 50;
    if (residence.has_radiator)     cost += 200;
    if (residence.has_closet)       cost += 100;
    if (residence.has_wallsocket)   cost += 25;
    if (residence.has_thermostatic) cost += 100;

    return cost;
}

void calculate_population_fitness(BumpAllocator *population_arena) {
    for (int i = 0; i < population_arena->items; i++) {
        Individual *individual = population_arena->get_individual_by_index(i);
        individual->fitness = calculate_residence_points(individual->residence);
    }
}

void select_individuals(
    BumpAllocator *population_arena,
    int nr_individuals,
    vector<pair<float, int>> *selected_individuals)
{
    // write the indices of the `nr_individuals` with the highest fitness to `selected_individuals`

    int total = population_arena->items;
    assert(nr_individuals <= total);

    // (fitness, index) pairs
    selected_individuals->reserve(total);

    for (int i = 0; i < total; i++) {
        float fitness = population_arena->get_individual_by_index(i)->fitness;
        selected_individuals->emplace_back(fitness, i);
    }

    sort(selected_individuals->begin(), selected_individuals->end(), [](auto &a, auto &b) {
        return a.first > b.first; //descending order
    });
}

void crossover(
    BumpAllocator* parent_allocator, 
    BumpAllocator* child_allocator, 
    const vector<pair<float, int>> *selected_individuals)
{
    constexpr float mutation_rate = 0.05f;

    for (int i = 0; i < N_SURVIVORS; ++i) {
        Individual* src = parent_allocator->get_individual_by_index(selected_individuals->at(i).second);
        Individual* dst = child_allocator->allocate(1);
        *dst = *src;
    }

    int remaining = POPULATION_SIZE - N_SURVIVORS;

    for (int i = 0; i < remaining; ++i) {
        int idx1 = selected_individuals->at(rand() % N_SURVIVORS).second;
        int idx2 = selected_individuals->at(rand() % N_SURVIVORS).second;

        Individual* p1 = parent_allocator->get_individual_by_index(idx1);
        Individual* p2 = parent_allocator->get_individual_by_index(idx2);
        Individual* child = child_allocator->allocate(1);

        auto mix = [&](bool a, bool b) -> bool {
            return rand() % 2 ? a : b;
        };

        auto mutate = [&](bool value) -> bool {
            return (static_cast<float>(rand()) / RAND_MAX < mutation_rate) ? !value : value;
        };

        Residence& r1 = p1->residence;
        Residence& r2 = p2->residence;
        Residence& rc = child->residence;

        // Enforce valid bath/shower configuration
        int bath_type = rand() % 3;
        rc.has_shower = (bath_type == 0);
        rc.has_bath = (bath_type == 1);
        rc.has_bath_and_shower = (bath_type == 2);

        // Apply crossover + mutation to remaining amenities
        rc.has_sink         = mutate(mix(r1.has_sink,         r2.has_sink));
        rc.has_dual_sink    = mutate(mix(r1.has_dual_sink,    r2.has_dual_sink));
        rc.has_curtain      = mutate(mix(r1.has_curtain,      r2.has_curtain));
        rc.has_radiator     = mutate(mix(r1.has_radiator,     r2.has_radiator));
        rc.has_closet       = mutate(mix(r1.has_closet,       r2.has_closet));
        rc.has_wallsocket   = mutate(mix(r1.has_wallsocket,   r2.has_wallsocket));
        rc.has_thermostatic = mutate(mix(r1.has_thermostatic, r2.has_thermostatic));

        child->fitness = 0.0f;
    }

    return;
};


int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    BumpAllocator current_generation_allocator;
    BumpAllocator next_generation_allocator;

    // (fitness, index) pairs
    vector<pair<float, int>> selected_individuals;

    current_generation_allocator.init(POPULATION_SIZE);
    next_generation_allocator.init(POPULATION_SIZE);

    generate_random_individuals(&current_generation_allocator, POPULATION_SIZE);

    cout << "Starting population: " << current_generation_allocator.items << "\n";

    for (int generation = 0; generation < GENERATIONS; generation++) {
        cout << "Generation " << generation + 1 << " starting...\n";

        calculate_population_fitness(&current_generation_allocator);

        // fill selection
        select_individuals(&current_generation_allocator, N_SURVIVORS, &selected_individuals);

        // crossover
        crossover(&current_generation_allocator, &next_generation_allocator, &selected_individuals);
        
        std::swap(current_generation_allocator, next_generation_allocator);
        next_generation_allocator.reset();

        cout << "Generation " << generation + 1<< " complete.\n";
        cout << display_individual(current_generation_allocator.get_individual_by_index(selected_individuals[0].second)) << "\n";
        cout << "\n";
    }

    cout << "Final population reached, selecting fittest individual..\n";
    calculate_population_fitness(&current_generation_allocator);
    select_individuals(&current_generation_allocator, 1, &selected_individuals);

    cout << display_individual(current_generation_allocator.get_individual_by_index(selected_individuals[0].second)) << "\n";

    return 0;
}
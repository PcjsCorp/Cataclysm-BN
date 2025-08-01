#pragma once

#include <climits>
#include <string>

#include "catacharset.h"
#include "color.h"
#include "translations.h"
#include "type_id.h"

using weather_effect_fn = std::function<void( int )>;

template<typename E> struct enum_traits;
template<typename T> class generic_factory;
class JsonObject;

enum class precip_class : int {
    none,
    very_light,
    light,
    medium,
    heavy,
    last
};
template<>
struct enum_traits<precip_class> {
    static constexpr precip_class last = precip_class::last;
};

enum class sun_intensity_type : int {
    none,
    light,
    normal,
    high,
    last
};
template<>
struct enum_traits<sun_intensity_type > {
    static constexpr sun_intensity_type last = sun_intensity_type::last;
};

enum class weather_time_requirement_type : int {
    day,
    night,
    both,
    last
};
template<>
struct enum_traits<weather_time_requirement_type> {
    static constexpr weather_time_requirement_type last = weather_time_requirement_type::last;
};


enum weather_sound_category : int {
    silent,
    drizzle,
    rainy,
    thunder,
    flurries,
    snowstorm,
    snow,
    last
};

template<>
struct enum_traits<weather_sound_category> {
    static constexpr weather_sound_category last = weather_sound_category::last;
};

/**
 * Weather animation class.
 */
struct weather_animation_t {
    float factor = 0.0f;
    nc_color color = c_white;
    std::string tile;
    uint32_t symbol = NULL_UNICODE;
    std::string get_symbol() const {
        return utf32_to_utf8( symbol );
    }
};

struct weather_requirements {
    int windpower_min = INT_MIN;
    int windpower_max = INT_MAX;
    int temperature_min = INT_MIN;
    int temperature_max = INT_MAX;
    int pressure_min = INT_MIN;
    int pressure_max = INT_MAX;
    int humidity_min = INT_MIN;
    int humidity_max = INT_MAX;
    bool humidity_and_pressure = true;
    bool acidic = false;
    weather_time_requirement_type time = weather_time_requirement_type::both;
    std::vector<weather_type_id> required_weathers;
};

struct weather_type {
    private:
        friend class generic_factory<weather_type>;

        bool was_loaded = false;
    public:
        weather_type_id id;
        translation name;           // UI name of weather type.
        nc_color color = c_magenta; // UI color of weather type.
        nc_color map_color = c_magenta; // Map color of weather type.
        uint32_t symbol = PERCENT_SIGN_UNICODE; // Map glyph of weather type.
        int ranged_penalty = 0;     // Penalty to ranged attacks.
        float sight_penalty = 1.0f; // Penalty to per-square visibility, applied in transparency map.
        int light_modifier = 0;     // Modification to ambient light.
        int sound_attn = 0;         // Sound attenuation of a given weather type.
        bool dangerous = false;     // If true, our activity gets interrupted.
        precip_class precip = precip_class::none; // Amount of associated precipitation.
        bool rains = false;         // Whether precipitation falls as rain.
        bool acidic = false;        // Whether precipitation is acidic.
        std::vector<std::pair<weather_effect_fn, int>> effects;
        weather_animation_t animation = {};
        weather_sound_category sound_category = weather_sound_category::silent;
        sun_intensity_type sun_intensity = sun_intensity_type::none;
        weather_requirements requirements = {};

        weather_type() = default;

        void load( const JsonObject &jo, const std::string &src );
        void check() const;

        std::string get_symbol() const {
            return utf32_to_utf8( symbol );
        }
};
namespace weather_types
{
/** Get all currently loaded weather types */
const std::vector<weather_type> &get_all();
/** Finalize all loaded weather types */
void finalize_all();
/** Clear all loaded weather types (invalidating any pointers) */
void reset();
/** Load weather type from JSON definition */
void load( const JsonObject &jo, const std::string &src );
/** Checks all loaded from JSON are valid */
void check_consistency();
} // namespace weather_types


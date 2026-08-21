#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include "chess_core.hpp"

inline sf::Vector2f snap(sf::Vector2f p){
    return sf::Vector2f(std::round(p.x), std::round(p.y));
}

inline void setCrispTextPosition(sf::Text& text, sf::Vector2f position){
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(snap(sf::Vector2f(position.x - bounds.left, position.y - bounds.top)));
}

// Visual board: rank 7 at the top unless flipped.
inline sf::Vector2f squareToPixel(const Square& square, float tile, sf::Vector2f origin, bool flip){
    const int visualRank = flip ? square.rank : (7 - square.rank);
    const int visualFile = flip ? (7 - square.file) : square.file;
    return sf::Vector2f(origin.x + visualFile * tile, origin.y + visualRank * tile);
}

inline std::optional<Square> pixelToSquare(sf::Vector2f point, float tile, sf::Vector2f origin, bool flip){
    const float x = point.x - origin.x;
    const float y = point.y - origin.y;
    if(x < 0 || y < 0) return std::nullopt;
    const int visualFile = static_cast<int>(x / tile);
    const int visualRank = static_cast<int>(y / tile);
    if(visualFile < 0 || visualFile > 7 || visualRank < 0 || visualRank > 7) return std::nullopt;
    const int file = flip ? (7 - visualFile) : visualFile;
    const int rank = flip ? visualRank : (7 - visualRank);
    return Square{file, rank};
}

inline sf::Color lighten(sf::Color color, int amount){
    const auto clampChannel = [](int value){ return std::clamp(value, 0, 255); };
    return sf::Color(
        static_cast<sf::Uint8>(clampChannel(static_cast<int>(color.r) + amount)),
        static_cast<sf::Uint8>(clampChannel(static_cast<int>(color.g) + amount)),
        static_cast<sf::Uint8>(clampChannel(static_cast<int>(color.b) + amount)),
        color.a
    );
}

float drawWrappedText(sf::RenderTarget& target,
                      const sf::Font& font,
                      const std::string& text,
                      unsigned characterSize,
                      sf::Vector2f position,
                      float maxWidth,
                      sf::Color color);

struct PieceAtlas {
    std::map<std::string, sf::Texture> tex;

    bool loadAll(const std::string& dir);
    const sf::Texture* get(const Piece& p) const;
};

enum class GameMode { Menu, PvP, PvAI, AIvAI };
std::string modeStr(GameMode m);

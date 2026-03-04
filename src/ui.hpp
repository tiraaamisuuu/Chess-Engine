#pragma once

#include "chess_core.hpp"

struct PieceAtlas {
    std::map<std::string, sf::Texture> tex;

    bool loadAll(const std::string& dir);
    const sf::Texture* get(const Piece& p) const;
};

enum class GameMode { Menu, PvP, PvAI, AIvAI };
std::string modeStr(GameMode m);

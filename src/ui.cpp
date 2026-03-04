#include "ui.hpp"

bool PieceAtlas::loadAll(const std::string& dir){
    const std::vector<std::string> colors = {"white_","black_"};
    const std::vector<std::string> names  = {"king","queen","rook","bishop","knight","pawn"};

    for(const auto& c : colors){
        for(const auto& n : names){
            std::string key = c+n;
            sf::Texture t;
            if(!t.loadFromFile(dir + "/" + key + ".png")) return false;
            t.setSmooth(false);
            tex.emplace(key, std::move(t));
        }
    }
    return true;
}

const sf::Texture* PieceAtlas::get(const Piece& p) const {
    auto k = pieceKey(p);
    auto it = tex.find(k);
    if(it==tex.end()) return nullptr;
    return &it->second;
}

std::string modeStr(GameMode m){
    switch(m){
        case GameMode::PvP: return "PvP";
        case GameMode::PvAI: return "PvAI";
        case GameMode::AIvAI: return "AIvAI";
        default: return "Menu";
    }
}

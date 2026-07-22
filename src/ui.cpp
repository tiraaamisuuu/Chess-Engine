#include "ui.hpp"

float drawWrappedText(sf::RenderTarget& target,
                      const sf::Font& font,
                      const std::string& text,
                      unsigned characterSize,
                      sf::Vector2f position,
                      float maxWidth,
                      sf::Color color)
{
    sf::Text rendered;
    rendered.setFont(font);
    rendered.setCharacterSize(characterSize);
    rendered.setFillColor(color);

    const float lineSpacing = font.getLineSpacing(characterSize);
    float y = position.y;

    auto flushLine = [&](const std::string& line){
        if(line.empty()) return;
        rendered.setString(line);
        setCrispTextPosition(rendered, sf::Vector2f(position.x, y));
        target.draw(rendered);
        y += lineSpacing;
    };

    auto fits = [&](const std::string& candidate){
        rendered.setString(candidate);
        return rendered.getLocalBounds().width <= maxWidth;
    };

    std::string line;
    std::string word;
    auto commitWord = [&](){
        if(word.empty()) return;
        const std::string candidate = line.empty() ? word : line + " " + word;
        if(fits(candidate)){
            line = candidate;
        } else {
            flushLine(line);
            line = word;
        }
        word.clear();
    };

    for(char ch : text){
        if(ch == '\n'){
            commitWord();
            flushLine(line);
            line.clear();
        } else if(ch == ' '){
            commitWord();
        } else {
            word.push_back(ch);
        }
    }

    commitWord();
    flushLine(line);
    return y - position.y;
}

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

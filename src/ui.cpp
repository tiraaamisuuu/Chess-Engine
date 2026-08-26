#include "ui.hpp"

namespace {
constexpr unsigned SoundSampleRate = 44100;

std::vector<sf::Int16> synthesiseCue(float durationSeconds,
                                     const std::function<float(float)>& oscillator){
    const size_t count = static_cast<size_t>(durationSeconds * SoundSampleRate);
    std::vector<sf::Int16> samples(count);
    for(size_t index = 0; index < count; index++){
        const float t = static_cast<float>(index) / static_cast<float>(SoundSampleRate);
        const float fadeIn = std::min(1.f, t / 0.004f);
        const float fadeOut = std::min(1.f, (durationSeconds - t) / 0.018f);
        const float envelope = fadeIn * std::max(0.f, fadeOut) * std::exp(-t * 13.f);
        const float value = std::clamp(oscillator(t) * envelope, -1.f, 1.f);
        samples[index] = static_cast<sf::Int16>(value * 24000.f);
    }
    return samples;
}

float sine(float frequency, float t){
    constexpr float Tau = 6.28318530718f;
    return std::sin(Tau * frequency * t);
}
}

ChessSoundSet::ChessSoundSet(){
    const auto moveSamples = synthesiseCue(0.095f, [](float t){
        return 0.58f * sine(690.f, t) + 0.28f * sine(410.f, t);
    });
    const auto captureSamples = synthesiseCue(0.14f, [](float t){
        const float transient = (t < 0.018f) ? sine(1480.f - 52000.f * t, t) : 0.f;
        return 0.52f * sine(250.f, t) + 0.30f * sine(420.f, t) + 0.28f * transient;
    });
    const auto checkSamples = synthesiseCue(0.22f, [](float t){
        const float secondTone = t > 0.055f ? sine(1040.f, t - 0.055f) : 0.f;
        return 0.46f * sine(780.f, t) + 0.40f * secondTone;
    });
    const auto gameOverSamples = synthesiseCue(0.38f, [](float t){
        return 0.30f * sine(392.f, t) + 0.30f * sine(494.f, t) + 0.30f * sine(587.f, t);
    });

    ready = moveBuffer.loadFromSamples(moveSamples.data(), moveSamples.size(), 1, SoundSampleRate) &&
            captureBuffer.loadFromSamples(captureSamples.data(), captureSamples.size(), 1, SoundSampleRate) &&
            checkBuffer.loadFromSamples(checkSamples.data(), checkSamples.size(), 1, SoundSampleRate) &&
            gameOverBuffer.loadFromSamples(gameOverSamples.data(), gameOverSamples.size(), 1, SoundSampleRate);
    if(!ready) return;

    moveSound.setBuffer(moveBuffer);
    captureSound.setBuffer(captureBuffer);
    checkSound.setBuffer(checkBuffer);
    gameOverSound.setBuffer(gameOverBuffer);
    moveSound.setVolume(46.f);
    captureSound.setVolume(50.f);
    checkSound.setVolume(43.f);
    gameOverSound.setVolume(42.f);
}

void ChessSoundSet::playMove(bool capture, bool check, bool gameOver){
    if(!ready) return;
    if(gameOver){
        gameOverSound.stop();
        gameOverSound.play();
    } else if(check){
        checkSound.stop();
        checkSound.play();
    } else if(capture){
        captureSound.stop();
        captureSound.play();
    } else {
        moveSound.stop();
        moveSound.play();
    }
}

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
    tex.clear();
    const std::vector<std::string> colors = {"white_","black_"};
    const std::vector<std::string> names  = {"king","queen","rook","bishop","knight","pawn"};

    for(const auto& c : colors){
        for(const auto& n : names){
            std::string key = c+n;
            sf::Texture t;
            if(!t.loadFromFile(dir + "/" + key + ".png")){
                tex.clear();
                return false;
            }
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
        case GameMode::PvP: return "pvp";
        case GameMode::PvAI: return "player vs Forklift";
        case GameMode::AIvAI: return "Forklift vs Forklift";
        default: return "menu";
    }
}

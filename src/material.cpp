/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <cstring>   // For std::memset

#include "material.h"
#include "thread.h"

using namespace std;

namespace Stockfish {

namespace {
  #define S(mg, eg) make_score(mg, eg)

  // Polynomial material imbalance parameters

  // One Score parameter for each pair (our piece, another of our pieces)
  constexpr Score QuadraticOurs[][PIECE_TYPE_NB] = {
    // OUR PIECE 2
    // bishop pair    pawn         knight       bishop       rook           queen
    {S(1419, 1455)                                                                  }, // Bishop pair
    {S( 101,   28), S( 37,  39)                                                     }, // Pawn
    {S(  57,   64), S(249, 187), S(-49, -62)                                        }, // Knight      OUR PIECE 1
    {S(   0,    0), S(118, 137), S( 10,  27), S(  0,   0)                           }, // Bishop
    {S( -63,  -68), S( -5,   3), S(100,  81), S(132, 118), S(-246, -244)            }, // Rook
    {S(-210, -211), S( 37,  14), S(147, 141), S(161, 105), S(-158, -174), S(-9,-31) }  // Queen
  };

  // One Score parameter for each pair (our piece, their piece)
  constexpr Score QuadraticTheirs[][PIECE_TYPE_NB] = {
    // THEIR PIECE
    // bishop pair   pawn         knight       bishop       rook         queen
    {                                                                               }, // Bishop pair
    {S(  33,  30)                                                                   }, // Pawn
    {S(  46,  18), S(106,  84)                                                      }, // Knight      OUR PIECE
    {S(  75,  35), S( 59,  44), S( 60,  15)                                         }, // Bishop
    {S(  26,  35), S(  6,  22), S( 38,  39), S(-12,  -2)                            }, // Rook
    {S(  97,  93), S(100, 163), S(-58, -91), S(112, 192), S(276, 225)               }  // Queen
  };

  #undef S

  // Endgame evaluation and scaling functions are accessed directly and not through
  // the function maps because they correspond to more than one material hash key.
  Endgame<KXK>    EvaluateKXK[] = { Endgame<KXK>(WHITE),    Endgame<KXK>(BLACK) };

  Endgame<KBPsK>  ScaleKBPsK[]  = { Endgame<KBPsK>(WHITE),  Endgame<KBPsK>(BLACK) };
  Endgame<KQKRPs> ScaleKQKRPs[] = { Endgame<KQKRPs>(WHITE), Endgame<KQKRPs>(BLACK) };
  Endgame<KPsK>   ScaleKPsK[]   = { Endgame<KPsK>(WHITE),   Endgame<KPsK>(BLACK) };
  Endgame<KPKP>   ScaleKPKP[]   = { Endgame<KPKP>(WHITE),   Endgame<KPKP>(BLACK) };


  /// imbalance() calculates the imbalance by comparing the piece count of each
  /// piece type for both colors.

  template<Color Us>
  Score imbalance(const int pieceCount[][PIECE_TYPE_NB]) {

    constexpr Color Them = ~Us;

    Score bonus = SCORE_ZERO;

    // Second-degree polynomial material imbalance, by Tord Romstad
    for (int pt1 = NO_PIECE_TYPE; pt1 <= QUEEN; ++pt1)
    {
        if (!pieceCount[Us][pt1])
            continue;

        int v = QuadraticOurs[pt1][pt1] * pieceCount[Us][pt1];

        for (int pt2 = NO_PIECE_TYPE; pt2 < pt1; ++pt2)
            v +=  QuadraticOurs[pt1][pt2] * pieceCount[Us][pt2]
                + QuadraticTheirs[pt1][pt2] * pieceCount[Them][pt2];

        bonus += pieceCount[Us][pt1] * v;
    }

    return bonus;
  }

} // namespace

namespace Material {


/// Material::probe() looks up the current position's material configuration in
/// the material hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same material configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.material_key();
  Entry* e = pos.this_thread()->materialTable[key];

  if (e->key == key)
      return e;

  std::memset(e, 0, sizeof(Entry));
  e->key = key;
  e->factor[WHITE] = e->factor[BLACK] = (uint8_t)SCALE_FACTOR_NORMAL;

  Value npm_w = pos.non_pawn_material(WHITE);
  Value npm_b = pos.non_pawn_material(BLACK);
  Value npm   = std::clamp(npm_w + npm_b, EndgameLimit, MidgameLimit);

  // Map total non-pawn material into [PHASE_ENDGAME, PHASE_MIDGAME]
  e->gamePhase = Phase(((npm - EndgameLimit) * PHASE_MIDGAME) / (MidgameLimit - EndgameLimit));

  // Evaluate the material imbalance. We use PIECE_TYPE_NONE as a place holder
  // for the bishop pair "extended piece", which allows us to be more flexible
  // in defining bishop pair bonuses.
  const int pieceCount[COLOR_NB][PIECE_TYPE_NB] = {
  { pos.count<BISHOP>(WHITE) > 1, pos.count<PAWN>(WHITE), pos.count<KNIGHT>(WHITE),
    pos.count<BISHOP>(WHITE)    , pos.count<ROOK>(WHITE), pos.count<QUEEN >(WHITE) },
  { pos.count<BISHOP>(BLACK) > 1, pos.count<PAWN>(BLACK), pos.count<KNIGHT>(BLACK),
    pos.count<BISHOP>(BLACK)    , pos.count<ROOK>(BLACK), pos.count<QUEEN >(BLACK) } };

  e->score = (imbalance<WHITE>(pieceCount) - imbalance<BLACK>(pieceCount)) / 16;
  return e;
}

} // namespace Material

} // namespace Stockfish

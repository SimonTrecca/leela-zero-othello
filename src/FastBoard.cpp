/*
    This file is part of Leela Zero.
    Copyright (C) 2017-2019 Gian-Carlo Pascutto and contributors

    Leela Zero is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leela Zero is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Leela Zero.  If not, see <http://www.gnu.org/licenses/>.

    Additional permission under GNU GPL version 3 section 7

    If you modify this Program, or any covered work, by linking or
    combining it with NVIDIA Corporation's libraries from the
    NVIDIA CUDA Toolkit and/or the NVIDIA CUDA Deep Neural
    Network library and/or the NVIDIA TensorRT inference library
    (or a modified version of those libraries), containing parts covered
    by the terms of the respective license agreement, the licensors of
    this Program grant you additional permission to convey the resulting
    work.
*/

#include "config.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>

#include "FastBoard.h"

#include "Utils.h"

using namespace Utils;

const int FastBoard::NBR_SHIFT;
const int FastBoard::NUM_VERTICES;
const int FastBoard::NO_VERTEX;
const int FastBoard::PASS;
const int FastBoard::RESIGN;


//this is a bitmask where each bit is a flag that identifies eyes on the go board
const std::array<int, 2> FastBoard::s_eyemask = {
    4 * (1 << (NBR_SHIFT * BLACK)),
    4 * (1 << (NBR_SHIFT * WHITE))
};

const std::array<FastBoard::vertex_t, 4> FastBoard::s_cinvert = {
    WHITE, BLACK, EMPTY, INVAL
};

//returns the board size
int FastBoard::get_boardsize() const {
    return m_boardsize;
}

//this returns the vertex number given x and y position
int FastBoard::get_vertex(const int x, const int y) const {
    assert(x >= 0 && x < BOARD_SIZE);
    assert(y >= 0 && y < BOARD_SIZE);
    assert(x >= 0 && x < m_boardsize);
    assert(y >= 0 && y < m_boardsize);

    int vertex = ((y + 1) * m_sidevertices) + (x + 1); 

    assert(vertex >= 0 && vertex < m_numvertices);

    return vertex;
}

//this returns the x and y coordinates given the vertex
std::pair<int, int> FastBoard::get_xy(const int vertex) const {
    // int vertex = ((y + 1) * (get_boardsize() + 2)) + (x + 1);
    int x = (vertex % m_sidevertices) - 1;
    int y = (vertex / m_sidevertices) - 1;

    assert(x >= 0 && x < m_boardsize);
    assert(y >= 0 && y < m_boardsize);
    assert(get_vertex(x, y) == vertex);

    return std::make_pair(x, y);
}

//returns the state of a vertex, which means whether it's black, white, empty, or invalid
FastBoard::vertex_t FastBoard::get_state(const int vertex) const {
    assert(vertex >= 0 && vertex < NUM_VERTICES);
    assert(vertex >= 0 && vertex < m_numvertices);

    return m_state[vertex];
}

//this sets the state of a vertex
void FastBoard::set_state(const int vertex, const FastBoard::vertex_t content) {
    assert(vertex >= 0 && vertex < NUM_VERTICES);
    assert(vertex >= 0 && vertex < m_numvertices);
    assert(content >= BLACK && content <= INVAL);

    m_state[vertex] = content;
}

//this calls the get_state function but takes the x and y coordinates, which means it calls the get_vertex function to get the vertex to send
FastBoard::vertex_t FastBoard::get_state(const int x, const int y) const {
    return get_state(get_vertex(x, y));
}

//same as stated above but for set_state
void FastBoard::set_state(const int x, const int y,
                          const FastBoard::vertex_t content) {
    set_state(get_vertex(x, y), content);
}

//this sets up the board from zero given the size
void FastBoard::reset_board(const int size) {
    m_boardsize = size;
    m_sidevertices = size + 2; //adds two to account for borders
    m_numvertices = m_sidevertices * m_sidevertices;
    m_tomove = BLACK;
    m_prisoners[BLACK] = 0;
    m_prisoners[WHITE] = 0;
    m_empty_cnt = 0; //counts the empty vertexes

    //these are the directions
    m_dirs[0] = -m_sidevertices;
    m_dirs[1] = +1;
    m_dirs[2] = +m_sidevertices;
    m_dirs[3] = -1;
    
    //sets up all the vertices as invalid
    for (int i = 0; i < m_numvertices; i++) {
        m_state[i] = INVAL;
        m_neighbours[i] = 0;
        m_parent[i] = NUM_VERTICES;
    }

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int vertex = get_vertex(i, j);

            m_state[vertex] = EMPTY; //sets up the vertex state as empty
            m_empty_idx[vertex] = m_empty_cnt; //since m_empty_cnt is used as the position in the m_empty vector, where all the empty vertexes are stored, you can use this to find the index of the vertex in m_empty
            m_empty[m_empty_cnt++] = vertex; //this adds the vertex to the list of empty ones then increases the m_empty_cnt value

            if (i == 0 || i == size - 1) { //this checks if it's on the top or bottom edge
                m_neighbours[vertex] += (1 << (NBR_SHIFT * BLACK))
                                      | (1 << (NBR_SHIFT * WHITE));
                m_neighbours[vertex] +=  1 << (NBR_SHIFT * EMPTY);
            } else {
                m_neighbours[vertex] +=  2 << (NBR_SHIFT * EMPTY); //if it's not a border vertex, it puts 2 in the bitmax in the zone dedicated to "empty" 
            }

            if (j == 0 || j == size - 1) {//this checks if it's on the right or left edge
                m_neighbours[vertex] += (1 << (NBR_SHIFT * BLACK))
                                      | (1 << (NBR_SHIFT * WHITE));
                m_neighbours[vertex] +=  1 << (NBR_SHIFT * EMPTY);
            } else {
                m_neighbours[vertex] +=  2 << (NBR_SHIFT * EMPTY); //if it's not a border vertex, it puts 2 in the bitmax in the zone dedicated to "empty" 
            }
        }
    }

    m_parent[NUM_VERTICES] = NUM_VERTICES;
    m_libs[NUM_VERTICES] = 16384; /* we will subtract from this */
    m_next[NUM_VERTICES] = NUM_VERTICES;

    assert(m_state[NO_VERTEX] == INVAL);
}

//This function checks if placing a stone at vertex 'i' by player of 'color' would result in a suicide move
bool FastBoard::is_suicide(const int i, const int color) const {
    // If there are liberties next to us, it is never suicide
    if (count_pliberties(i)) {
        return false;
    }

    // If we get here, we played in a "hole" surrounded by stones
    for (auto k = 0; k < 4; k++) {
        auto ai = i + m_dirs[k]; //m_dirs represents the directions, so m[0] is the vertex up, m[1] is to the right, m[2] is at the bottom, and m[3] is to the left

        auto libs = m_libs[m_parent[ai]]; //it checks for the liberties of the group
        if (get_state(ai) == color) { //if this adjacent is our same color
            if (libs > 1) { //and it has more than 1 liberty 
                // connecting to live group = not suicide
                return false;
            }
        } else if (get_state(ai) == !color) { //if this adjacent is our opponent's color
            if (libs <= 1) { //and it has less or 1 liberty
                // killing neighbour = not suicide
                return false;
            }
        }
    }

    // We played in a hole, friendlies had one liberty at most and
    // we did not kill anything. So we killed ourselves.
    return true;
}

//checks how many empty neighbours the vertex has
int FastBoard::count_pliberties(const int i) const {
    return count_neighbours(EMPTY, i);
}

// count neighbours of color c at vertex v
// the border of the board has fake neighours of both colors
int FastBoard::count_neighbours(const int c, const int v) const {
    assert(c == WHITE || c == BLACK || c == EMPTY);
    return (m_neighbours[v] >> (NBR_SHIFT * c)) & NBR_MASK; //this returns the number of neighbours of color c by checking the neighbours bitmask, shifting to the color we want to check, then using the NBR_MASK to isolate the ones we need
}

//this adds a vertex, which means it needs to update all its neighbours' data
void FastBoard::add_neighbour(const int vtx, const int color) {
    assert(color == WHITE || color == BLACK || color == EMPTY);

    std::array<int, 4> nbr_pars; 
    int nbr_par_cnt = 0;

    for (int k = 0; k < 4; k++) {
        int ai = vtx + m_dirs[k]; //iterates on its neighbours

        m_neighbours[ai] += (1 << (NBR_SHIFT * color))
                          - (1 << (NBR_SHIFT * EMPTY)); //removes an empty, adds one of the color of the pawn we added

        bool found = false;
        for (int i = 0; i < nbr_par_cnt; i++) {
            if (nbr_pars[i] == m_parent[ai]) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_libs[m_parent[ai]]--; //removes a liberty from the group
            nbr_pars[nbr_par_cnt++] = m_parent[ai];
        }
    }
}

//does the same thing as function above, but it removes instead of adding
void FastBoard::remove_neighbour(const int vtx, const int color) {
    assert(color == WHITE || color == BLACK || color == EMPTY);

    std::array<int, 4> nbr_pars;
    int nbr_par_cnt = 0;

    for (int k = 0; k < 4; k++) {
        int ai = vtx + m_dirs[k]; 

        m_neighbours[ai] += (1 << (NBR_SHIFT * EMPTY))
                          - (1 << (NBR_SHIFT * color));

        bool found = false;
        for (int i = 0; i < nbr_par_cnt; i++) {
            if (nbr_pars[i] == m_parent[ai]) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_libs[m_parent[ai]]++; //adds a liberty to the group
            nbr_pars[nbr_par_cnt++] = m_parent[ai];
        }
    }
}

//returns how many vertexes of the given color are reachable on the board
int FastBoard::calc_reach_color(const int color) const {
    auto reachable = 0; //counts the reachable vertices of a given color on the board
    auto bd = std::vector<bool>(m_numvertices, false); //marks visited vertices, defaults at false
    auto open = std::queue<int>(); //initializes a queue to add vertexes of the same color in
    for (auto i = 0; i < m_boardsize; i++) {
        for (auto j = 0; j < m_boardsize; j++) {
            auto vertex = get_vertex(i, j);
            if (m_state[vertex] == color) {
                reachable++;
                bd[vertex] = true; 
                open.push(vertex);
            }
        }
    }
    while (!open.empty()) { //for each vertex we found on the board with the color we're looking for
        /* colored field, spread */
        auto vertex = open.front();
        open.pop();

        for (auto k = 0; k < 4; k++) {
            auto neighbor = vertex + m_dirs[k]; //it checks its neighbours
            if (!bd[neighbor] && m_state[neighbor] == EMPTY) { //if a neighbour hasn't been explored yet and is empty
                reachable++; //then it becomes reachable
                bd[neighbor] = true;
                open.push(neighbor);//and it's added to the queue 
            }
        }
    }
    return reachable;
}

// Needed for scoring passed out games not in MC playouts
float FastBoard::area_score(const float komi) const {
    auto white = calc_reach_color(WHITE);
    auto black = calc_reach_color(BLACK);
    return black - white - komi;
}

//This function displays the board, marking the last move played
void FastBoard::display_board(const int lastmove) {
    int boardsize = get_boardsize();

    myprintf("\n   ");
    print_columns();
    for (int j = boardsize - 1; j >= 0; j--) {
        myprintf("%2d", j + 1);
        if (lastmove == get_vertex(0, j))
            myprintf("(");
        else
            myprintf(" ");
        for (int i = 0; i < boardsize; i++) {
            if (get_state(i, j) == WHITE) {
                myprintf("O");
            } else if (get_state(i, j) == BLACK) {
                myprintf("X");
            } else if (starpoint(boardsize, i, j)) {
                myprintf("+");
            } else {
                myprintf(".");
            }
            if (lastmove == get_vertex(i, j)) {
                myprintf(")");
            } else if (i != boardsize - 1 && lastmove == get_vertex(i, j) + 1) {
                myprintf("(");
            } else {
                myprintf(" ");
            }
        }
        myprintf("%2d\n", j + 1);
    }
    myprintf("   ");
    print_columns();
    myprintf("\n");
}

//prints the columns
void FastBoard::print_columns() {
    for (int i = 0; i < get_boardsize(); i++) {
        if (i < 25) {
            myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
        } else {
            myprintf("%c ", (('A' + (i - 25) < 'I') ? 'A' + (i - 25)
                                                    : 'A' + (i - 25) + 1));
        }
    }
    myprintf("\n");
}

//when two groups of stones merge, this function merges them into one parent and updates the parents of all the stones in both groups
void FastBoard::merge_strings(const int ip, const int aip) {
    assert(ip != NUM_VERTICES && aip != NUM_VERTICES);

    /* merge stones */
    m_stones[ip] += m_stones[aip];

    /* loop over stones, update parents */
    int newpos = aip;

    do {
        // check if this stone has a liberty
        for (int k = 0; k < 4; k++) {
            int ai = newpos + m_dirs[k];
            // for each liberty, check if it is not shared
            if (m_state[ai] == EMPTY) {
                // find liberty neighbors
                bool found = false;
                for (int kk = 0; kk < 4; kk++) {
                    int aai = ai + m_dirs[kk];
                    // friendly string shouldn't be ip
                    // ip can also be an aip that has been marked
                    if (m_parent[aai] == ip) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    m_libs[ip]++;
                }
            }
        }

        m_parent[newpos] = ip;
        newpos = m_next[newpos];
    } while (newpos != aip);

    /* merge stings */
    std::swap(m_next[aip], m_next[ip]);
}

//this checks if there is an eye pattern
bool FastBoard::is_eye(const int color, const int i) const {
    /* check for 4 neighbors of the same color */
    int ownsurrounded = (m_neighbours[i] & s_eyemask[color]);

    // if not, it can't be an eye
    // this takes advantage of borders being colored
    // both ways
    if (!ownsurrounded) {
        return false;
    }

    // 2 or more diagonals taken
    // 1 for side groups
    int colorcount[4];

    colorcount[BLACK] = 0;
    colorcount[WHITE] = 0;
    colorcount[INVAL] = 0;

    colorcount[m_state[i - 1 - m_sidevertices]]++;
    colorcount[m_state[i + 1 - m_sidevertices]]++;
    colorcount[m_state[i - 1 + m_sidevertices]]++;
    colorcount[m_state[i + 1 + m_sidevertices]]++;

    if (colorcount[INVAL] == 0) {
        if (colorcount[!color] > 1) {
            return false;
        }
    } else {
        if (colorcount[!color]) {
            return false;
        }
    }

    return true;
}
//converts the move to text
std::string FastBoard::move_to_text(const int move) const {
    std::ostringstream result;

    int column = move % m_sidevertices;
    int row = move / m_sidevertices;

    column--;
    row--;

    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (column >= 0 && column < m_boardsize));

    if (move >= 0 && move <= m_numvertices) {
        result << static_cast<char>(column < 8 ? 'A' + column
                                               : 'A' + column + 1);
        result << (row + 1);
    } else if (move == FastBoard::PASS) {
        result << "pass";
    } else if (move == FastBoard::RESIGN) {
        result << "resign";
    } else {
        result << "error";
    }

    return result.str();
}

//converts the text to a move
int FastBoard::text_to_move(std::string move) const {
    transform(cbegin(move), cend(move), begin(move), tolower);

    if (move == "pass") {
        return PASS;
    } else if (move == "resign") {
        return RESIGN;
    } else if (move.size() < 2 || !std::isalpha(move[0])
               || !std::isdigit(move[1]) || move[0] == 'i') {
        return NO_VERTEX;
    }

    auto column = move[0] - 'a';
    if (move[0] > 'i') {
        --column;
    }

    int row;
    std::istringstream parsestream(move.substr(1));
    parsestream >> row;
    --row;

    if (row >= m_boardsize || column >= m_boardsize) {
        return NO_VERTEX;
    }

    return get_vertex(column, row);
}

//it converts a move into sgf text, the format used for recording GO games
std::string FastBoard::move_to_text_sgf(const int move) const {
    std::ostringstream result;

    int column = move % m_sidevertices;
    int row = move / m_sidevertices;

    column--;
    row--;

    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (column >= 0 && column < m_boardsize));

    // SGF inverts rows
    row = m_boardsize - row - 1;

    if (move >= 0 && move <= m_numvertices) {
        if (column <= 25) {
            result << static_cast<char>('a' + column);
        } else {
            result << static_cast<char>('A' + column - 26);
        }
        if (row <= 25) {
            result << static_cast<char>('a' + row);
        } else {
            result << static_cast<char>('A' + row - 26);
        }
    } else if (move == FastBoard::PASS) {
        result << "tt";
    } else if (move == FastBoard::RESIGN) {
        result << "tt";
    } else {
        result << "error";
    }

    return result.str();
}

bool FastBoard::starpoint(const int size, const int point) {
    int stars[3];
    int points[2];
    int hits = 0;

    if (size % 2 == 0 || size < 9) {
        return false;
    }

    stars[0] = size >= 13 ? 3 : 2;
    stars[1] = size / 2;
    stars[2] = size - 1 - stars[0];

    points[0] = point / size;
    points[1] = point % size;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            if (points[i] == stars[j]) {
                hits++;
            }
        }
    }

    return hits >= 2;
}

bool FastBoard::starpoint(const int size, const int x, const int y) {
    return starpoint(size, y * size + x);
}

int FastBoard::get_prisoners(const int side) const {
    assert(side == WHITE || side == BLACK);

    return m_prisoners[side];
}

int FastBoard::get_to_move() const {
    return m_tomove;
}

bool FastBoard::black_to_move() const {
    return m_tomove == BLACK;
}

bool FastBoard::white_to_move() const {
    return m_tomove == WHITE;
}

void FastBoard::set_to_move(const int tomove) {
    m_tomove = tomove;
}

//This returns a connected group of stones starting from vertex
std::string FastBoard::get_string(const int vertex) const {
    std::string result;

    int start = m_parent[vertex];
    int newpos = start;

    do {
        result += move_to_text(newpos) + " ";
        newpos = m_next[newpos];
    } while (newpos != start);

    // eat last space
    assert(result.size() > 0);
    result.resize(result.size() - 1);

    return result;
}

std::string FastBoard::get_stone_list() const {
    std::string result;

    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {
            int vertex = get_vertex(i, j);

            if (get_state(vertex) != EMPTY) {
                result += move_to_text(vertex) + " ";
            }
        }
    }

    // eat final space, if any.
    if (result.size() > 0) {
        result.resize(result.size() - 1);
    }

    return result;
}

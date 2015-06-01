#include <iostream>
#include <vector>
#include <limits>
#include <cstdlib>
#include <cstring>

#include "ttt3d.h"

//our namespace
namespace MZ {

/*
	Made by Mark Sabini & Zachary Kaplan
	Completed on 06/01/15
	Assignment: TTT3D
	Class: Data Structures
*/

using namespace std;

typedef unsigned long long ull;

enum class ABType : char { //set underlying integer type to char because we only have two values (char is smallest size 1 byte{
	MAX, MIN //Maximizer or minimizer node
};

struct ABNode {
	ABNode(ABType type, ull P, ull E) : type(type), P(P), E(E)
	{
		P_next = 0;
		E_next = 0;
		if(type == ABType::MAX) score = std::numeric_limits<int>::min();
		else if(type == ABType::MIN) score = std::numeric_limits<int>::max();
	}

	vector<ABNode*> children;
	ull	P, //State associated with each node
		E,
		P_next, //Best next possible move
		E_next;
	int score;
	ABType type;
};

//FOR DYNAMIC EVAL
struct box {
        //const static values for determing score (as of now P1 is 1 more than E2, this is to, for example, prioritize creating a fork over blocking the creation of one)
        static constexpr uint32_t
                MIN = 0,
                MAX = numeric_limits<uint32_t>::max(),
                MID = MAX / 2,
                P1 = MID / 7,           //notice P1 > P2 but 2(P2) > P1
                P2 = P1 * 3 / 4,
                E2 = MID / 7 - 1,       //notice E1 << E2 (specifically 7(E1) < E2)
                E1 = E2 / 7 - 1;

        box() : p(0), e(0), numP1(0), numP2(0), numE1(0), numE2(0), loss(0), pos(0), score(0) {}
        box(const box &b) : p(b.p), e(b.e), numP1(b.numP1), numP2(b.numP2), numE1(b.numE1), numE2(b.numE2), loss(b.loss), pos(b.pos), score(b.score) {}
        box &operator=(const box &b) {  //this is not a true copy, just what's needed (should only be used in bucket sort)
                loss = b.loss; //probably not needed
                pos = b.pos;
                score = b.score;
        }

        void reset(unsigned char pos) {
                p = e = numP1 = numP2 = numE1 = numE2 = loss = 0;
                score = MID;
                this->pos = pos;
        }

        unsigned char
                p     : 1,      //1 if a player is in the spot, else 0
                e     : 1,      //-------enemy -----------------------
                numP1 : 3,      //the number of lines going through this point that contain exactly one player piece
                numP2 : 3,      //------------------------------------------------------------------two-------------s
                numE1 : 3,      //------------------------------------------------------------------one enemy ------
                numE2 : 3,      //------------------------------------------------------------------two-------------s
                loss  : 1,      //0 if normal box, but 1 if moving here will avert a loss
                      : 1,      //padding bit to avoid issues with byte borders
                pos;            //holds position in array, important after sorting (0-63, but letting be full size to avoid masking commands)
        uint32_t score : 32;    //score, higher is better (using uint32_t because I can sort by this key quickly using bucket sort)
};


//index in the array represents power of 2 in ULL format we're using for the board
static box scores[64] = {}, temp[64] = {}, *scoresEnd= scores + 64, *te = temp + 64;


class MZ : public TTT3D {
	public:
		ull get_P(){ return P; }
		ull get_E(){ return E; }
		MZ(const duration<double> total_time_allowed, int gamemode = 4) : TTT3D(total_time_allowed), gamemode(gamemode) { //Default to dynamic randomized minimax
			history.reserve(64);
			current_line = 0;
		}
		void next_move(int mv[3]) { //Takes an int array of size 3, and then changes the array to indicate next move
		 //If we go first, then we will be passed an illegal move
		 	srand(time(NULL));
			if(mv[0] >= 0 && mv[0] <= 3 && mv[1] >= 0 && mv[1] <= 3 && mv[2] >= 0 && mv[2] <= 3) 
			{
				history.push_back(foreign_convert(mv)); //I.e. we didn't go first
				E = E | foreign_convert(mv); //Update our record of the enemy state
			}
			else history.push_back(0);
			//Write code for finding the move here
			ull move = 0;

			//MARK: make sure you attempt to place a winning move before blocking an enemies win next turn. Winning this move should be top priority
			//I can't tell which order this is in because I'm unfamiliar with atari, but make sure you try to win before averting loss (next turn won't happen if we win now)
			if(ull i = atari(P, E))
			{
				move = i;
				//cout << "Atari\n";
			}
			else if(ull i = atari(E, P))
			{
				move = i;
				//cout << "Atari2\n";
			}
			else if(gamemode == 0) //Controlled line-searching (deterministic in a sense)
			{
				if(current_line == 0 || current_line & E) //No current line yet, or enemy has played in line
				{
					vector<ull> free_center_lines;
					for(const ull *i = wins; i < winEnd; ++i)
					{
						if(!(*i & E) && (
									!((*i >> 21) & 1) ||
								 	!((*i >> 22) & 1) || 
									!((*i >> 25) & 1) || 
									!((*i >> 26) & 1) || 
									!((*i >> 37) & 1) ||
									!((*i >> 38) & 1) ||
									!((*i >> 41) & 1) || 
									!((*i >> 42) & 1) )) //spaced out for readability
						{
							free_center_lines.push_back(*i);
						}
					}
					if(!free_center_lines.empty())
					{
						/*sort(free_center_lines.begin(), free_center_lines.end(), compare_P_presence);
						reverse(free_center_lines.begin(), free_center_lines.end());
						current_line = free_center_lines.at(0);*/
						current_line = free_center_lines.at(rand() % free_center_lines.size());
						//cout << "Got one\n";
					}
					else
					{
						//cout << "Regular lines\n";
						vector<ull> free_lines;
						for(const ull *i = wins; i < winEnd; ++i)
						{
							if(!(*i & E)) free_lines.push_back(*i);
						}
						if(!free_lines.empty())
						{
							current_line = free_lines.at(rand() % free_lines.size());
						}
						else
						{
							gamemode = 1;
							goto gamemode1; //careful with large goto's like this. This looks fine, but g++ can get made about this kinda thing
						}
					}
				}
				vector<ull> free_spots;
				ull poss = current_line - (current_line & P);
				ull indicator = 1;
				while(poss > 0)
				{
					if(poss & 1) free_spots.push_back(indicator);
					indicator <<= 1;
					poss >>= 1;
				}
				if(!free_spots.empty()) 
				{
					move = free_spots.at(rand() % free_spots.size());
				//	cout << "have a move\n";
				}
			}
			else if(gamemode == 1) //Alpha-beta pruning based
			{
				gamemode1:
					//cout << "\nEntering Alpha-Beta\n";
				ABNode *root = new ABNode(ABType::MAX, P, E);
				gen_tree(root, 3);
				AB(root, 3);
				ull next = root->P_next;
				move = root->P_next - root->P;
				//cout << "P_next " << root->P_next << "\n";
			}
			else if(gamemode == 2) //Absolutely berserk
			{
				ull state = P | E;
				vector<ull> free_spots;
				ull indicator = 1;
				for(int i = 0; i < 64; ++i)
				{
					if(!(state & 1)) //the spot is free
					{
						free_spots.push_back(indicator);
					}
					indicator <<= 1;
				}
				if(!free_spots.empty())
				{
					int chosen = rand() % free_spots.size();
					move = free_spots.at(indicator);
				}
			}
			else if(gamemode == 3) //Only dynamic evaluation
			{
				eval2(P, E);
				move = scores[63].pos;
			}
			else if(gamemode == 4) //Dynamic evaluation with minimax
			{
				ABNode *root = new ABNode(ABType::MAX, P, E);
				gen_tree(root, 3);
				AB2(root, 3);
				ull next = root->P_next;
				move = root->P_next - root->P;
			}
			P = P | move; //Update our record of our state
			foreign_convert(move, mv); //Store the new move.
			//cout << 16*mv[2] + 4*mv[1] + mv[0] << "\n";
			history.push_back(move); //reserve (max 64 moves)
		}
		int won()
		{
			if(won(P)) return 1;
			if(won(E)) return 2;
			else return 0;
		}
	private:
		int gamemode; //0 for line-searching, 1 for alpha-beta pruning, 2 for berserk
		ull current_line;
		ull P = 0, E = 0; //storage for board, set to 0 initially
		vector<ull> history; 	//History of all moves made by players. The first value is 0 if we went first. Else it just starts.

		static const char MAX_DEPTH = 4, //Maximum depth for search (moved from constructor so it is set compile time
		     		  WINS = 76;
		static constexpr ull wins[WINS] = {
			15ULL, 240ULL, 3840ULL, 61440ULL, 983040ULL, 15728640ULL, 251658240ULL, 4026531840ULL, 64424509440ULL, 1030792151040ULL, 16492674416640ULL, 263882790666240ULL,
			4222124650659840ULL, 67553994410557440ULL, 1080863910568919040ULL, 17293822569102704640ULL, 4369ULL, 8738ULL, 17476ULL, 34952ULL, 286326784ULL, 572653568ULL,
			1145307136ULL, 2290614272ULL, 18764712116224ULL, 37529424232448ULL, 75058848464896ULL, 150117696929792ULL, 1229764173248856064ULL, 2459528346497712128ULL,
			4919056692995424256ULL, 9838113385990848512ULL, 281479271743489ULL, 562958543486978ULL, 1125917086973956ULL, 2251834173947912ULL, 4503668347895824ULL, 
			9007336695791648ULL, 18014673391583296ULL, 36029346783166592ULL, 72058693566333184ULL, 144117387132666368ULL, 288234774265332736ULL, 576469548530665472ULL,
			1152939097061330944ULL, 2305878194122661888ULL, 4611756388245323776ULL, 9223512776490647552ULL, 33825ULL, 4680ULL, 2216755200ULL, 306708480ULL, 145277268787200ULL,
			20100446945280ULL, 9520891087237939200ULL, 1317302891005870080ULL, 1152922604119523329ULL, 281543712968704ULL, 2305845208239046658ULL, 563087425937408ULL,
			4611690416478093316ULL, 1126174851874816ULL, 9223380832956186632ULL, 2252349703749632ULL, 2251816993685505ULL, 281483566907400ULL, 36029071898968080ULL,
			4503737070518400ULL, 576465150383489280ULL, 72059793128294400ULL, 9223442406135828480ULL, 1152956690052710400ULL, 9223376434903384065ULL, 1152923703634296840ULL, 
			281612482805760ULL, 2252074725150720ULL
		}, *winEnd = const_cast<ull *>(wins) + WINS; //not sure why it's forcing me to use a const_cast..


		static void foreign_convert(ull u, int mv[3])
		{
			int pwr = 0;
			while(u > 1)
			{
				u >>= 1;
				++pwr;
			}
			mv[2] = pwr >> 4; 			// pwr/16
			mv[1] = (pwr - (mv[2] << 4)) >> 2;  	// (pwr - 16*mv[2]) / 4
			mv[0] = pwr & 3;			// pwr % 4
		}
		static ull foreign_convert(int mv[3])
		{
			return (1ULL) << ( (mv[2] << 4) + (mv[1] << 2) + mv[0] ); //The power exponent of the ull. (Originally 16*mv[2] + 4*mv[1] + mv[0])
		}
		static bool is_valid(ull A, ull B) //Returns true if there are no intersections
		{
			return !(A & B);
		}
		static bool won(ull P)
		{
			for(const ull *i = wins; i < winEnd; ++i)
			{
				if(won(P, *i)) return true;
			}
			return false;
		}
		static bool won(ull P, ull w)
		{
			return (P & w) == w;
		}
		static ull atari(ull P, ull E) //Essentially, the method returns the spot where player P should play to avoid losing to E.
		{ //Returns 0 if safe, i.e. no atari.
			for(const ull *i = wins; i < winEnd; ++i)
			{
				ull u = E & *i,
					u_ = u;
				int counter = 0;
				while(u_ > 0)
				{
					if(u_ & 1) ++counter;
					u_ >>= 1;
				}
				if(counter == 3)
				{
					ull leftover = *i - u;
					if(!(P & leftover)) 
					{
						return leftover;
					}
				}
			}
			return 0; //There is no atari.
		}
		static ull eval(ull P, ull E) //Static evaluation function
		{
			int P_wins = (won(P)) ? 1 : 0;
			int E_wins = (won(E)) ? 1 : 0;
			int P_ataris = atari(P);
			int E_ataris = atari(E);
			int P_twos = two(P);
			int E_twos = two(E);
			int P_potential = 76*76*76*P_wins + 76*P_ataris + P_twos;
			int E_potential = 76*76*76*E_wins + 76*76*E_ataris + 2*E_twos;
			return P_potential - E_potential;
		}
		static void prune(ABNode *root, int i) //Cuts off all other trees after i //Unfortunately it doesn't work.
		{
			/*if(root->children.size() > i + 1)
			{
				for(int j = i + 1; j < root->children.size(); ++j)
				{
					delete(root->children[j]);
				}
				cout << "prune\n";
			}*/
		}
		static void AB(ABNode *root, int depth, int alpha, int beta) //To make this easier, also pass up the game states (so the AI knows which move to take next)
		{ //(This isn't actually AB since it doesn't prune...oh well...
			if(root->children.empty()) 
			{
				//cout << "EVAL\n";
				root->score = eval(root->P, root->E);
			}
			switch(root->type) { //added a switch statement instead of two if's (better practice for enums)

			case ABType::MAX:
				for(int i = 0; i < root->children.size(); ++i)
				{
					AB(root->children[i], depth + 1, alpha, beta);
					int alpha_ = root->children[i]->score;
					if(alpha_ > alpha)
					{
						alpha = alpha_;
						root->score = alpha;
						(root->P_next) = root->children[i]->P;
						(root->E_next) = root->children[i]->E;
					}
					else if(alpha_ == alpha)
					{
						int r = rand() % 3;
						if(r == 0)
						{
							alpha = alpha_;
							root->score = alpha;
							(root->P_next) = root->children[i]->P;
							(root->E_next) = root->children[i]->E;
						}
					}
					if(beta <= alpha) prune(root, i);
					//cout << root->children.size() + 1 << " " << i + 1 << "\n";
				}
				return;
			case ABType::MIN:
				for(int i = 0; i < root->children.size(); ++i)
				{
					AB(root->children[i], depth + 1, alpha, beta);
					int beta_ = root->children[i]->score;
					if(beta_ < beta)
					{
						beta = beta_;
						root->score = beta;
						(root->P_next) = root->children[i]->P;
						(root->E_next) = root->children[i]->E;
					}
					else if(beta_ == beta)
					{
						int r = rand() % 3;
						if(r == 0)
						{
							beta = beta_;
							root->score = beta;
							(root->P_next) = root->children[i]->P;
							(root->E_next) = root->children[i]->E;
						}
					}
					if(beta <= alpha) prune(root, i);
					//cout << root->children.size() + 1 << " " << i + 1 << "\n";
				}
				return;
			}
		}
		static void AB2(ABNode *root, int depth, int alpha, int beta) //To make this easier, also pass up the game states (so the AI knows which move to take next)
		{ //(This isn't actually AB since it doesn't prune...oh well...
			if(root->children.empty()) 
			{
				//cout << "EVAL\n";
				eval2(root->P, root->E);
				root->score = scores[63].pos;
			}
			switch(root->type) { //added a switch statement instead of two if's (better practice for enums)

			case ABType::MAX:
				for(int i = 0; i < root->children.size(); ++i)
				{
					AB(root->children[i], depth + 1, alpha, beta);
					int alpha_ = root->children[i]->score;
					if(alpha_ > alpha)
					{
						alpha = alpha_;
						root->score = alpha;
						(root->P_next) = root->children[i]->P;
						(root->E_next) = root->children[i]->E;
					}
					else if(alpha_ == alpha)
					{
						int r = rand() % 3;
						if(r == 0)
						{
							alpha = alpha_;
							root->score = alpha;
							(root->P_next) = root->children[i]->P;
							(root->E_next) = root->children[i]->E;
						}
					}
					if(beta <= alpha) prune(root, i);
					//cout << root->children.size() + 1 << " " << i + 1 << "\n";
				}
				return;
			case ABType::MIN:
				for(int i = 0; i < root->children.size(); ++i)
				{
					AB(root->children[i], depth + 1, alpha, beta);
					int beta_ = root->children[i]->score;
					if(beta_ < beta)
					{
						beta = beta_;
						root->score = beta;
						(root->P_next) = root->children[i]->P;
						(root->E_next) = root->children[i]->E;
					}
					else if(beta_ == beta)
					{
						int r = rand() % 3;
						if(r == 0)
						{
							beta = beta_;
							root->score = beta;
							(root->P_next) = root->children[i]->P;
							(root->E_next) = root->children[i]->E;
						}
					}
					if(beta <= alpha) prune(root, i);
				}
				return;
			}
		}
		static void gen_tree(ABNode *root, int depth, ABType p = ABType::MAX) //Optimize this to get rid of symmetric cases
		{
			if(depth > 0)
			{
				ull state = root->P | root->E;
				ull indicator = 1;
				if(!won(root->P) && !won(root->E))
				{
					for(int i = 0; i < 64; ++i)
					{
						if(!(state & 1)) //i.e. it's empty
						{	
							ABType newstate = p == ABType::MAX ? ABType::MIN : ABType::MAX;
							ABNode *n;
							if(p == ABType::MAX)
							{
								n = new ABNode(ABType::MIN, (root->P) | indicator, root->E);
							}
							else
							{
								n = new ABNode(ABType::MAX, root->P, (root->E | indicator));
							}
							root->children.push_back(n);
							gen_tree(n, depth - 1, newstate);
						}
						indicator <<= 1;
						state >>= 1;
					}
				}
			}
		}
		static void AB(ABNode *root, int depth) //NOTE: This does not return the value, rather the next move (encoded in ull) for the AI to make.
		{
			AB(root, depth, numeric_limits<int>::min(), numeric_limits<int>::max());
		}
		static void AB2(ABNode *root, int depth) //NOTE: This does not return the value, rather the next move (encoded in ull) for the AI to make.
		{
			AB2(root, depth, numeric_limits<int>::min(), numeric_limits<int>::max());
		}

		static int ull_sum(ull u) //Returns digital sum of ull in binary
		{
			int counter = 0;
			while(u > 0)
			{
				if(u & 1) ++counter;
				u >>= 1;
			}
			return counter;
		}
		
		//And these functions calculate things about game states:
		static int atari(ull state) //returns the number of atari's in a given board state
		{
			int count = 0;
			for(const ull *i = wins; i < winEnd; ++i)
			{
				if(ull_sum(state & *i) >= 3) ++count;
			}
			return count;
		}
		
		static int center(ull state) //returns the number of cubes occupied in the center 8
		{
			ull c = 0;
			c += 1ULL << 21; c += 1ULL << 22; c += 1ULL << 25; c += 1ULL << 26;
			c += 1ULL << 37; c += 1ULL << 38; c += 1ULL << 41; c += 1ULL << 42;
			return ull_sum(state & c);
		}
		
		static int corner(ull state) //returns the number of corners occupied
		{
			ull c = 0;
			c += 1ULL << 0; c += 1ULL << 3; c += 1ULL << 12; c += 1ULL << 15;
			c += 1ULL << 48; c += 1ULL << 51; c += 1ULL << 60; c += 1ULL << 63;
			return ull_sum(state & c);
		}
		
		static int moves(ull state) //returns number of moves so far (+- 1)
		{
			return ull_sum(state);
		}
		
		static int two(ull state) //returns the number of rows with 2 occupied positions
		{
			int count = 0;
			for(const ull *i = wins; i < winEnd; ++i)
			{
				if(ull_sum(state & *i) == 2) ++count;
			}
			return count;	
		}
		
		static int free(ull P, ull E) //returns the number of rows that are completely empty
		{
			int count = 0;
			ull state = P | E;
			for(const ull *i = wins; i < winEnd; ++i)
			{
				if(!(state & *i)) ++count;
			}
			return count;
		}
		
		//STUFF FOR DYNAMIC EVAL
		
		static void reset_all(box *s, box *e) {
			for(unsigned char i = 0; s < e; ++s, ++i)
				s->reset(i);
		}

		//specialized and messy DO NOT USE FOR ANYTHING ELSE
		static void bucket_sort(box *s, box *e) {
			uint32_t buckets[0x10001] = {}, *be = buckets + 0x10001, *b;
			box *p, *t;

			for(p = s; p != e; ++p) //count frequency
				++buckets[1 + (p->score & 0xFFFF)];
		
			for(b = buckets + 1; b != be; ++b) //make list of indexes
				*b += *(b-1);

			for(p = s; p != e; ++p) //sort once
				temp[buckets[p->score & 0xFFFF]++] = *p;
			
			memset(buckets, 0, 0x10001 * 4); //reset bucket
	
			for(t = temp; t != te; ++t)
				++buckets[1 + (t->score >> 16)];

			for(b = buckets + 1; b != be; ++b)
				*b += *(b-1);

			for(t = temp; t != te; ++t)
				s[buckets[t->score >> 16]++] = *t; //this line requires random access iterator
		}
		
		//Modifies scores
		//Scores will be in reverse order of importance, search as deep as you want, start from the back
		//IF RETURN IS TRUE:
		//	Tail of scores is a move that will win the game or attempt to save the game (avert loss), just move there
		//ELSE:
		//	Scores is sorted in increasing score order, essentially best moves come last (pos in each box is the bit shift required to get move)
		static bool eval2(ull P, ull E) {
			box *pts[4], **p; 	// pointers to the four boxes in the line, **p is an iterator
					ull cp_p, cp_e;		// holds the combinations of each line and current board state for player and enemy
			char pwr, np, ne;	// holds the power of the current shift, the number of players in the line, and the number of enemies in the line respectively
			reset_all(scores, scoresEnd);

			for(ull win : wins) {
				cp_p = P;
				cp_e = E;
				p = pts;
				pwr = np = ne = 0;
		
				//to account for some spaces being geometrically better, we will add one to each space each time we go over it

				while(win > 0) {
					if(win & 1) {
						*p = scores + pwr;
						if((*p)->loss) //shouldn't tamper with these
							goto skip;

						if(cp_p & 1) {
							(*p)->p = 1;
							(*p)->score = box::MIN;
							++np;
						} else if(cp_e & 1) {
							(*p)->e = 1;
							(*p)->score = box::MIN;
							++ne;
						} else {
							++((*p)->score);
						}
skip:
						++p;
					}
					++pwr;
					win >>= 1;
					cp_p >>= 1;
					cp_e >>= 1;
				}

				for(box *pt : pts) {
					if(pt->p || pt->e || pt->loss) //if a piece is here or if already loss aversion state, no need to adjust score or any other values
						continue;
					//if(pt->pos == 4)
					//	cout << (int)np << ", " << (int)ne << endl;
					if(np) {
						if(!ne) {	//just players in line
							switch(np) {
							case 1:
								if(!pt->numP2 && !pt->numP1)	//first helpful line through this box
									pt->score += box::P1;
								else if(pt->numP2 < 2) 		//either 1 or 0 (if 0 then numP1 > 0), so it's an intersection of 1 and 1 or 1 and 2 but not 2 and 2
									pt->score -= box::P2;
								else				//numP2 >= 2 so this is a good move (creates a fork and a 2 in this line)
									pt->score += box::P2;

								++(pt->numP1);
								break;
							case 2:
								if(!pt->numP2 && !pt->numP1)	//first helpul line through this box
									pt->score += box::P2;
								else if(pt->numP2)		//FORKKK!!!!
									pt->score += box::P1 + box::P1;
								else				//Intersection of 2 and 1
									pt->score -= box::P2;

								++(pt->numP2);
								break;
							case 3:	//WE WON!!!!!!!
								scores[63] = *pt;
								return true;
							}
						}		//else is both, but nothing to do
					} else if (ne) {	//just enemies in line
						switch(ne) {
						case 1:
							pt->score += box::E1;
							++(pt->numE1);
							break;
						case 2:
							if(pt->numE2 < 2) //cap out enemy score contribution after fork of two (fork of three etc. is just as dangerous, nothing special)
								pt->score += box::E2;
							++(pt->numE2);
							break;
						case 3:	//This could avert a loss, keep looking though in case there is a win state 
							pt->score = box::MAX;
							pt->loss = 1;
						}
					}			//else is neither, but will never happen (taken care of above)
				}
			}
		
			bucket_sort(scores, scoresEnd);
			return scores[0].loss;
		}

};

}

constexpr unsigned long long MZ::MZ::wins[MZ::MZ::WINS];


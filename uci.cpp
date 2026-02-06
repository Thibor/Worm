#include "program.h"

//Get index of key
static int UciIndex(vector<string> list, string command) {
	for (size_t n = 0; n < list.size() - 1; n++)
		if (list[n] == command)
			return n;
	return -1;
}

//get next word after uci command
static bool UciValue(vector<string> list, string command, string& value) {
	value = "";
	for (size_t n = 0; n < list.size() - 1; n++)
		if (list[n] == command) {
			value = list[n + 1];
			return true;
		}
	return false;
}

//get next words after uci command
static bool UciValues(vector<string> list, string command, string& value) {
	bool result = false;
	value = "";
	for (size_t n = 0; n < list.size(); n++) {
		if (result)
			value += " " + list[n];
		else if (list[n] == command)
			result = true;
	}
	value = Trim(value);
	return result;
}

static void UciEval() {
	ShowEval();
}

static void UciPonderhit() {
	info.ponder = false;
	info.post = true;
	info.stop = false;
	info.timeStart = GetTimeMs();
}

static void UciQuit() {
	exit(0);
}

static void UciStop() {
	info.stop = true;
}

//Performance test
static inline void PerftDriver(int depth) {
	int count;
	Move list[256];
	pos.MoveList(pos.ColorUs(), list, count);
	for (int i = 0; i < count; i++){
		pos.MakeMove(list[i]);
		if (depth)
			PerftDriver(depth - 1);
		else
			info.nodes++;
		pos.UnmakeMove(list[i]);
	}
}

static void ResetInfo() {
	info.stop = false;
	info.post = true;
	info.nodes = 0;
	info.multiPV = 1;
	info.depthLimit = (Depth)64;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
	info.bestMove = MOVE_NONE;
	info.ponderMove = MOVE_NONE;
	info.rootMoves.clear();
}

static int ShrinkNumber(U64 n) {
	if (n < 10000)
		return 0;
	if (n < 10000000)
		return 1;
	if (n < 10000000000)
		return 2;
	return 3;
}

void PrintPerformanceHeader() {
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}

//displays a summary
static void PrintSummary(U64 time, U64 nodes) {
	if (time < 1)
		time = 1;
	U64 nps = (nodes * 1000) / time;
	const char* units[] = { "", "k", "m", "g" };
	int sn = ShrinkNumber(nps);
	U64 p = pow(10, sn * 3);
	printf("-----------------------------\n");
	printf("Time        : %llu\n", time);
	printf("Nodes       : %llu\n", nodes);
	printf("Nps         : %llu (%llu%s/s)\n", nps, nps / p, units[sn]);
	printf("-----------------------------\n");
}

//start benchamrk test
static void UciBench() {
	ResetInfo();
	PrintPerformanceHeader();
	pos.SetFen();
	info.depthLimit = DEPTH_ZERO;
	info.post = false;
	S64 elapsed = 0;
	while (elapsed < 3000)
	{
		++info.depthLimit;
		SearchIteratively();
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(GetTimeMs() - info.timeStart, info.nodes);
}

//start performance test
static void UciPerformance(){
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	pos.SetFen();
	while (GetTimeMs() - info.timeStart < 3000)
	{
		PerftDriver(info.depthLimit++);
		printf(" %2d. %8llu %12llu\n", info.depthLimit, GetTimeMs() - info.timeStart, info.nodes);
	}
	PrintSummary(GetTimeMs() - info.timeStart, info.nodes);
}

static void ParsePosition(string command) {
	string fen = START_FEN;
	stringstream ss(command);
	string token;
	ss >> token;
	if (token != "position")
		return;
	ss >> token;
	if (token == "startpos")
		ss >> token;
	else if (token == "fen") {
		fen = "";
		while (ss >> token && token != "moves")
			fen += token + " ";
		fen.pop_back();
	}
	pos.SetFen(fen);
	while (ss >> token) {
		CMoveList list = CMoveList(pos);
		for (Move m : list)
			if (m.ToUci() == token) {
				pos.MakeMove(m);
				break;
			}
	}
}

static void ParseGo(string command) {
	stringstream ss(command);
	string token;
	ss >> token;
	if (token != "go")
		return;
	ResetInfo();
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movestogo = 32;
	while (ss >> token) {
		if (token == "ponder")
			info.ponder = true;
		else if(token == "searchmoves") {
			CMoveList list = CMoveList(pos);
			while (ss >> token) 
				for (Move m : list)
					if (m.ToUci() == token)
						info.rootMoves.push_back(m);
		}
		else if (token == "wtime")
			ss >> wtime;
		else if (token == "btime")
			ss >> btime;
		else if (token == "winc")
			ss >> winc;
		else if (token == "binc")
			ss >> binc;
		else if (token == "movestogo")
			ss >> movestogo;
		else if (token == "movetime")
			ss >> info.timeLimit;
		else if (token == "depth")
			ss >> info.depthLimit;
		else if (token == "nodes")
			ss >> info.nodesLimit;
	}
	int time = pos.color ? btime : wtime;
	int inc = pos.color ? binc : winc;
	if (time)
		info.timeLimit = min(time / movestogo + inc, time / 2);
	SearchIteratively();
}

//supports all uci commands
void UciCommand(string str) {
	string value;
	vector<string> split{};
	SplitString(str, split,' ');
	Move m;
	if (split.empty())
		return;
	string command = split[0];
	if (command == "uci")
	{
		int delta = 50;
		cout << "id name " << NAME << endl;
		cout << "option name hash type spin default " << options.hash << " min 1 max 1024" << endl;
		cout << "option name MultiPV type spin default 1 min 1 max 32" << endl;
		printf("option name UCI_Elo type spin default %d min %d max %d\n", options.eloMax, options.eloMin, options.eloMax);
		printf("option name rfp type spin default %d min %d max %d\n", options.rfp, options.rfp - delta, options.rfp + delta);
		printf("option name futility type spin default %d min %d max %d\n", options.futility, options.futility - delta, options.futility + delta);
		printf("option name razoring type spin default %d min %d max %d\n", options.razoring, options.razoring - delta, options.razoring + delta);
		printf("option name null type spin default %d min %d max %d\n", options.nullMove, options.nullMove - delta, options.nullMove + delta);
		printf("option name LMR type spin default %d min %d max %d\n", options.lmr, options.lmr - delta, options.lmr + delta);
		printf("option name aspiration type spin default %d min %d max %d\n", options.aspiration, options.aspiration - delta, options.aspiration + delta);
		cout << "option name ponder type check default " << (options.ponder ? "true" : "false") << endl;
		cout << "option name material type string default " << options.material << endl;
		cout << "option name mobility type string default " << options.mobility << endl;
		cout << "option name passed type string default " << options.passed << endl;
		cout << "option name pawn type string default " << options.pawn << endl;
		cout << "option name rook type string default " << options.rook << endl;
		cout << "option name king type string default " << options.king << endl;
		cout << "option name outpost type string default " << options.outpost << endl;
		cout << "option name bishop type string default " << options.bishop << endl;
		cout << "option name defense type string default " << options.defense << endl;
		cout << "option name outFile type string default " << options.outFile << endl;
		cout << "option name outRank type string default " << options.outRank << endl;
		cout << "option name tempo type string default " << options.tempo << endl;
		cout << "uciok" << endl;
	}
	else if (command == "isready")
		cout << "readyok" << endl;
	else if (command == "ucinewgame") {
		tt.Clear();
		InitEval();
		InitSearch();
	}
	else if (command == "position") 
		ParsePosition(str);
	else if (command == "go") 
		ParseGo(str);
	else if (command == "ponderhit")
		UciPonderhit();
	else if (command == "quit")
		UciQuit();
	else if (command == "stop")
		UciStop();
	else if (command == "setoption")
	{
		string name;
		bool isValue = UciValues(split, "value", value);
		if (isValue && UciValue(split, "name", name)) {
			name = StrToLower(name);
			if (name == "ponder")
				options.ponder = value == "true";
			else if (name == "hash")
				tt.Resize(stoi(value));
			else if (name == "multipv")
				options.multiPV = stoi(value);
			else if (name == "uci_elo")
				options.elo = stoi(value);
			else if (name == "rfp")
				options.rfp = stoi(value);
			else if (name == "futility")
				options.futility = stoi(value);
			else if (name == "razoring")
				options.razoring = stoi(value);
			else if (name == "null")
				options.nullMove = stoi(value);
			else if (name == "lmr")
				options.lmr = stoi(value);
			else if (name == "aspiration")
				options.aspiration = stoi(value);
			else if (name == "tempo")
				options.tempo = value;
			else if (name == "material")
				options.material = value;
			else if (name == "mobility")
				options.mobility = value;
			else if (name == "outfile")
				options.outFile = value;
			else if (name == "outrank")
				options.outRank = value;
			else if (name == "passed")
				options.passed = value;
			else if (name == "pawn")
				options.pawn = value;
			else if (name == "rook")
				options.rook = value;
			else if (name == "king")
				options.king = value;
			else if (name == "outpost")
				options.outpost = value;
			else if (name == "bishop")
				options.bishop = value;
			else if (name == "defense")
				options.defense = value;
		}
	}
	else if (command == "bench")
		UciBench();
	else if (command == "perft")
		UciPerformance();
	else if (command == "eval")
		UciEval();
	else if (command == "print")
		pos.PrintBoard();
}

//main uci loop
void UciLoop() {
	//pos.SetFen("8/8/8/8/1q6/3k4/8/2K5 w - - 14 68");
	//UciCommand("go depth 2");
	string line;
	while (true) {
		getline(cin, line);
		UciCommand(line);
	}
}
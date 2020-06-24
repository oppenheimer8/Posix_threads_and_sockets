struct st_dat_ord{

	int ord;

};

struct st_dat_res{

	int res;

};

enum tipo_ord {
	DETECTAR,
	ATERRIZAR,
	ACABAR
};

struct st_ord {
	enum tipo_ord orden;
	struct st_dat_ord datos;
};

enum tipo_res {
	ACK,
	DATOS,
	EN_TIERRA
};
struct st_res {
	enum tipo_res result;
	int n_vaa;
	struct st_dat_res datos;
};

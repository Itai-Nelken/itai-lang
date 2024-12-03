/*

fn print(i: i32) {
	// ...
}

fn main() -> i32 {
	for var i = 0; i < 10; i++ {
		defer print(i);
	}
	return 0;
}
*/


/*** runtime ***/
#include <stdlib.h>

struct capture {
	void (*callback)(struct capture *);
};

struct capture_array {
	struct capture **data;
	unsigned cap;
	unsigned len;
};


struct capture *__new_capture(void (*cb)(struct capture*), unsigned size) {
	struct capture *c = calloc(1, size);
	c->callback = cb;
	return c;
}

void __capture_array_push(struct capture_array *a, struct capture *c) {
	if(a->data == NULL || a->len + 1 > a->cap) {
		a->cap = (a->cap == 0 ? 2 : a->cap * 2);
		a->data = realloc(a->data, sizeof(*a->data) * a->cap);
	}
	a->data[a->len++] = c;
}

#define CAPTURE_ARRAY_INIT (struct capture_array){.data = NULL, .cap = 0, .len = 0}
#define CAPTURE_ARRAY_FREE(a) (free(a.data), a.data = NULL, a.cap = a.len = 0)

#define DEF_CAPTURE(name, callback_body, ...) struct capture_##name { struct capture h; __VA_ARGS__ }; static void capture_##name##_callback(struct capture *capture) { struct capture_##name *c = (struct capture_##name *)capture; callback_body }

/*** end runtime ***/

// pre-declarations
void print(int);
int main(void);
// end pre-declarations

// captures

DEF_CAPTURE(main_defer0, {print(c->value);}, int value;)

// end captures

#include <stdio.h>
void print(int i) {
	printf("%d\n", i);
}

int main(void) {
	// prolog
	int __return_value;
	struct capture_array __defers = CAPTURE_ARRAY_INIT;

	// body
	for(int i = 0; i < 10; ++i) {
		{
			struct capture_main_defer0 *c = (struct capture_main_defer0 *)__new_capture(capture_main_defer0_callback, sizeof(*c));
			c->value = i;
			__capture_array_push(&__defers, (struct capture *)c);
		}
	}
	__return_value = 0;
	goto _fn_main_end;
	// end body

	// epilogue
_fn_main_end:
	for(int i = __defers.len; i > 0; --i) {
		__defers.data[i - 1]->callback(__defers.data[i - 1]);
		free(__defers.data[i - 1]);
	}
	CAPTURE_ARRAY_FREE(__defers);
	return __return_value;
}


#ifndef EXTERN_H
#define EXTERN_H

__BEGIN_DECLS

int	 khttp_input_parent(int fd, struct kreq *r);
void	 khttp_input_child(void);

__END_DECLS

#endif

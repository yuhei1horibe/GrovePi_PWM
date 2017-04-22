/**
 * @author Y.Horibe
 * Definition of packet format for GrovePi
 */

#include <stdint.h>

union Packet_t{
	uint32_t		data;
	struct Field_t{
		uint32_t	cmd		: 8;
		uint32_t	pin		: 8;
		uint32_t	data1	: 8;
		uint32_t	data2	: 8;
	}field;
};


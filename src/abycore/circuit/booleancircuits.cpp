/**
 \file 		booleancircuits.cpp
 \author	michael.zohner@ec-spride.de
 \copyright	ABY - A Framework for Efficient Mixed-protocol Secure Two-party Computation
			Copyright (C) 2015 Engineering Cryptographic Protocols Group, TU Darmstadt
			This program is free software: you can redistribute it and/or modify
			it under the terms of the GNU Affero General Public License as published
			by the Free Software Foundation, either version 3 of the License, or
			(at your option) any later version.
			This program is distributed in the hope that it will be useful,
			but WITHOUT ANY WARRANTY; without even the implied warranty of
			MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
			GNU Affero General Public License for more details.
			You should have received a copy of the GNU Affero General Public License
			along with this program. If not, see <http://www.gnu.org/licenses/>.
 \brief		A collection of boolean circuits for boolean and yao sharing in the ABY framework
 */

#include "booleancircuits.h"

void BooleanCircuit::Init() {
	m_nShareBitLen = 1;
	m_nNumANDSizes = 1;
	m_vANDs = (non_lin_vec_ctx*) malloc(sizeof(non_lin_vec_ctx) * m_nNumANDSizes);
	m_vANDs[0].bitlen = 1;
	m_vANDs[0].numgates = 0;

	//Instantiate with regular 1 output AND gate
	m_vTTlens.resize(1);
	m_vTTlens[0].resize(1);
	m_vTTlens[0][0].resize(1);
	m_vTTlens[0][0][0].tt_len = 4;
	m_vTTlens[0][0][0].numgates = 0;
	m_vTTlens[0][0][0].out_bits = 1;

	//m_vTTlens = (non_lin_vec_ctx*) malloc(sizeof(tt_lens_ctx) * m_nNumTTSizes);

	m_nGates = 0;
	if (m_eContext == S_BOOL) {
		m_nRoundsAND = 1;
		m_nRoundsXOR = 0;
		m_nRoundsIN.resize(2, 1);
		m_nRoundsOUT.resize(3, 1);
	} else if(m_eContext == S_BOOL_NO_MT) {
		m_nRoundsAND = 1;
		m_nRoundsXOR = 0;
		m_nRoundsIN.resize(2, 1);
		m_nRoundsOUT.resize(3, 1);
	} else if (m_eContext == S_YAO || m_eContext == S_YAO_REV) {
		m_nRoundsAND = 0;
		m_nRoundsXOR = 0;
		m_nRoundsIN.resize(2);
		m_nRoundsIN[0] = 1;
		m_nRoundsIN[1] = 2;
		m_nRoundsOUT.resize(3, 1);
		m_nRoundsOUT[1] = 0; //the client already holds the output bits from the start
	} else {
		cerr << "Sharing type not implemented for Boolean circuit" << endl;
		exit(0);
	}

	m_nB2YGates = 0;
	m_nA2YGates = 0;
	m_nNumXORVals = 0;
	m_nNumXORGates = 0;
	m_nYSwitchGates = 0;

}

/*void BooleanCircuit::UpdateANDsOnLayers() {

}*/

void BooleanCircuit::Cleanup() {
	//TODO implement
}

uint32_t BooleanCircuit::PutANDGate(uint32_t inleft, uint32_t inright) {
	uint32_t gateid;
	if(m_eContext != S_BOOL_NO_MT) {
		gateid = m_cCircuit->PutPrimitiveGate(G_NON_LIN, inleft, inright, m_nRoundsAND);

		if (m_eContext == S_BOOL) {
			UpdateInteractiveQueue(gateid);
		} else if (m_eContext == S_YAO || m_eContext == S_YAO_REV) {
			//if context == YAO, no communication round is required
			UpdateLocalQueue(gateid);
		} else {
			cerr << "Context not recognized" << endl;
		}

		if (m_pGates[gateid].nvals != INT_MAX) {
			m_vANDs[0].numgates += m_pGates[gateid].nvals;
		} else {
			cerr << "INT_MAX not allowed as nvals" << endl;
		}
	} else {
		vector<uint32_t> in(2);
		uint64_t andttable=8;
		in[0] = inleft;
		in[1] = inright;
		gateid = PutTruthTableGate(in, 1, &andttable);
	}
	return gateid;
}

vector<uint32_t> BooleanCircuit::PutANDGate(vector<uint32_t> inleft, vector<uint32_t> inright) {
	PadWithLeadingZeros(inleft, inright);
	uint32_t lim = inleft.size();
	vector<uint32_t> out(lim);
	for (uint32_t i = 0; i < lim; i++)
		out[i] = PutANDGate(inleft[i], inright[i]);
	return out;
}

share* BooleanCircuit::PutANDGate(share* ina, share* inb) {
	return new boolshare(PutANDGate(ina->get_wires(), inb->get_wires()), this);
}

uint32_t BooleanCircuit::PutVectorANDGate(uint32_t choiceinput, uint32_t vectorinput) {
	if (m_eContext != S_BOOL) {
		cerr << "Building a vector AND gate is currently only possible for GMW!" << endl;
		//TODO: prevent error by putting repeater gate on choiceinput and an AND gate between choiceinput and vectorinput
		return 0;
	}


	uint32_t gateid = m_cCircuit->PutNonLinearVectorGate(G_NON_LIN_VEC, choiceinput, vectorinput, m_nRoundsAND);
	UpdateInteractiveQueue(gateid);

	//cout << "Putting a vector and gate between a gate with " << m_pGates[choiceinput].nvals << " and " <<
	//		m_pGates[vectorinput].nvals << ", res gate has nvals = " << m_pGates[gateid].nvals << endl;


	if (m_pGates[gateid].nvals != INT_MAX) {
		//Update vector AND sizes
		//find location of vector AND bitlength
		//int pos = FindBitLenPositionInVec(m_pGates[gateid].nvals, m_vANDs, m_nNumANDSizes);
		int pos = FindBitLenPositionInVec(m_pGates[gateid].gs.avs.bitlen, m_vANDs, m_nNumANDSizes);
		if (pos == -1) {
			//Create new entry for the bit-length
			m_nNumANDSizes++;
			non_lin_vec_ctx* temp = (non_lin_vec_ctx*) malloc(sizeof(non_lin_vec_ctx) * m_nNumANDSizes);
			memcpy(temp, m_vANDs, (m_nNumANDSizes - 1) * sizeof(non_lin_vec_ctx));
			free(m_vANDs);
			m_vANDs = temp;
			//m_vANDs[m_nNumANDSizes - 1].bitlen = m_pGates[gateid].nvals;
			m_vANDs[m_nNumANDSizes - 1].bitlen = m_pGates[gateid].gs.avs.bitlen;
			m_vANDs[m_nNumANDSizes - 1].numgates = m_pGates[choiceinput].nvals; //1
		} else {
			//increase number of vector ANDs for this bitlength by one
			m_vANDs[pos].numgates+=m_pGates[choiceinput].nvals;
		}
	}
	return gateid;
}

share* BooleanCircuit::PutXORGate(share* ina, share* inb) {
	return new boolshare(PutXORGate(ina->get_wires(), inb->get_wires()), this);
}

uint32_t BooleanCircuit::PutXORGate(uint32_t inleft, uint32_t inright) {
	//cout << "inleft = " << inleft << ", inright = " << inright << endl;
	uint32_t gateid = m_cCircuit->PutPrimitiveGate(G_LIN, inleft, inright, m_nRoundsXOR);
	UpdateLocalQueue(gateid);
	m_nNumXORVals += m_pGates[gateid].nvals;
	m_nNumXORGates += 1;
	return gateid;
}

vector<uint32_t> BooleanCircuit::PutXORGate(vector<uint32_t> inleft, vector<uint32_t> inright) {
	PadWithLeadingZeros(inleft, inright);
	uint32_t lim = inleft.size();
	vector<uint32_t> out(lim);
	for (uint32_t i = 0; i < lim; i++)
		out[i] = PutXORGate(inleft[i], inright[i]);
	return out;
}

uint32_t BooleanCircuit::PutINGate(e_role src) {
	uint32_t gateid = m_cCircuit->PutINGate(m_eContext, 1, m_nShareBitLen, src, m_nRoundsIN[src]);
	UpdateInteractiveQueue(gateid);
	switch (src) {
	case SERVER:
		m_vInputGates[0].push_back(gateid);
		m_vInputBits[0] += m_pGates[gateid].nvals;
		break;
	case CLIENT:
		m_vInputGates[1].push_back(gateid);
		m_vInputBits[1] += m_pGates[gateid].nvals;
		break;
	case ALL:
		m_vInputGates[0].push_back(gateid);
		m_vInputGates[1].push_back(gateid);
		m_vInputBits[0] += m_pGates[gateid].nvals;
		m_vInputBits[1] += m_pGates[gateid].nvals;
		break;
	default:
		cerr << "Role not recognized" << endl;
		break;
	}

	return gateid;
}

uint32_t BooleanCircuit::PutSIMDINGate(uint32_t ninvals, e_role src) {
	uint32_t gateid = m_cCircuit->PutINGate(m_eContext, ninvals, m_nShareBitLen, src, m_nRoundsIN[src]);
	UpdateInteractiveQueue(gateid);
	switch (src) {
	case SERVER:
		m_vInputGates[0].push_back(gateid);
		m_vInputBits[0] += m_pGates[gateid].nvals;
		break;
	case CLIENT:
		m_vInputGates[1].push_back(gateid);
		m_vInputBits[1] += m_pGates[gateid].nvals;
		break;
	case ALL:
		m_vInputGates[0].push_back(gateid);
		m_vInputGates[1].push_back(gateid);
		m_vInputBits[0] += m_pGates[gateid].nvals;
		m_vInputBits[1] += m_pGates[gateid].nvals;
		break;
	default:
		cerr << "Role not recognized" << endl;
		break;
	}

	return gateid;
}


share* BooleanCircuit::PutDummyINGate(uint32_t bitlen) {
	vector<uint32_t> wires(bitlen);
	for(uint32_t i = 0; i < bitlen; i++) {
		wires[i] = PutINGate((e_role) !m_eMyRole);
	}
	return new boolshare(wires, this);
}
share* BooleanCircuit::PutDummySIMDINGate(uint32_t nvals, uint32_t bitlen) {
	vector<uint32_t> wires(bitlen);
	for(uint32_t i = 0; i < bitlen; i++) {
		wires[i] = PutSIMDINGate(nvals, (e_role) !m_eMyRole);
	}
	return new boolshare(wires, this);
}


uint32_t BooleanCircuit::PutSharedINGate() {
	uint32_t gateid = m_cCircuit->PutSharedINGate(m_eContext, 1, m_nShareBitLen);
	UpdateLocalQueue(gateid);
	return gateid;
}

uint32_t BooleanCircuit::PutSharedSIMDINGate(uint32_t ninvals) {
	uint32_t gateid = m_cCircuit->PutSharedINGate(m_eContext, ninvals, m_nShareBitLen);
	UpdateLocalQueue(gateid);
	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutINGate(T val) {

	uint32_t gateid = PutINGate(m_eMyRole);
	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.ishare.inval = (UGATE_T*) calloc(1 * m_nShareBitLen, sizeof(UGATE_T));

	*gate->gs.ishare.inval = (UGATE_T) val;
	gate->instantiated = true;

	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutSharedINGate(T val) {

	uint32_t gateid = PutSharedINGate();
	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.val = (UGATE_T*) calloc(1 * m_nShareBitLen, sizeof(UGATE_T));

	*gate->gs.val = (UGATE_T) val;
	gate->instantiated = true;

	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutSIMDINGate(uint32_t ninvals, T val) {

	uint32_t gateid = PutSIMDINGate(ninvals, m_eMyRole);
	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.ishare.inval = (UGATE_T*) calloc(ninvals * m_nShareBitLen, sizeof(UGATE_T));

	*gate->gs.ishare.inval = (UGATE_T) val;
	gate->instantiated = true;

	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutSharedSIMDINGate(uint32_t ninvals, T val) {

	uint32_t gateid = PutSharedSIMDINGate(ninvals);
	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.val = (UGATE_T*) calloc(ninvals * m_nShareBitLen, sizeof(UGATE_T));

	*gate->gs.val = (UGATE_T) val;
	gate->instantiated = true;

	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutINGate(T* val, e_role role) {
	uint32_t gateid = PutINGate(role);
	if (role == m_eMyRole) {
		//assign value
		GATE* gate = m_pGates + gateid;
		gate->gs.ishare.inval = (UGATE_T*) calloc(ceil_divide(1 * m_nShareBitLen, GATE_T_BITS), sizeof(UGATE_T));
		memcpy(gate->gs.ishare.inval, val, ceil_divide(1 * m_nShareBitLen, 8));

		gate->instantiated = true;
	}
	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutSharedINGate(T* val) {
	uint32_t gateid = PutSharedINGate();

		//assign value
		GATE* gate = m_pGates + gateid;
		gate->gs.val = (UGATE_T*) calloc(ceil_divide(1 * m_nShareBitLen, GATE_T_BITS), sizeof(UGATE_T));
		memcpy(gate->gs.val, val, ceil_divide(1 * m_nShareBitLen, 8));

		gate->instantiated = true;

	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutSIMDINGate(uint32_t ninvals, T* val, e_role role) {
	uint32_t gateid = PutSIMDINGate(ninvals, role);
	if (role == m_eMyRole) {
		//assign value
		GATE* gate = m_pGates + gateid;
		gate->gs.ishare.inval = (UGATE_T*) calloc(ceil_divide(ninvals * m_nShareBitLen, GATE_T_BITS), sizeof(UGATE_T));
		memcpy(gate->gs.ishare.inval, val, ceil_divide(ninvals * m_nShareBitLen, 8));
		gate->instantiated = true;
	}
	return gateid;
}


template<class T> uint32_t BooleanCircuit::PutSharedSIMDINGate(uint32_t ninvals, T* val) {
	uint32_t gateid = PutSharedSIMDINGate(ninvals);
	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.val = (UGATE_T*) calloc(ceil_divide(ninvals * m_nShareBitLen, GATE_T_BITS), sizeof(UGATE_T));
	memcpy(gate->gs.val, val, ceil_divide(ninvals * m_nShareBitLen, 8));
	gate->instantiated = true;
	return gateid;
}


uint32_t BooleanCircuit::PutINGate(uint64_t val, e_role role) {
	//return PutINGate(nvals, &val, role);
	uint32_t gateid = PutINGate(role);
	if (role == m_eMyRole) {
		//assign value
		GATE* gate = m_pGates + gateid;
		gate->gs.ishare.inval = (UGATE_T*) calloc(ceil_divide(1 * m_nShareBitLen, sizeof(UGATE_T) * 8), sizeof(UGATE_T));
		memcpy(gate->gs.ishare.inval, &val, ceil_divide(1 * m_nShareBitLen, 8));

		gate->instantiated = true;
	}

	return gateid;
}


uint32_t BooleanCircuit::PutSharedINGate(uint64_t val) {
	uint32_t gateid = PutSharedINGate();
	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.val = (UGATE_T*) calloc(ceil_divide(1 * m_nShareBitLen, sizeof(UGATE_T) * 8), sizeof(UGATE_T));
	memcpy(gate->gs.val, &val, ceil_divide(1 * m_nShareBitLen, 8));

	gate->instantiated = true;
	return gateid;
}

uint32_t BooleanCircuit::PutSIMDINGate(uint32_t nvals, uint64_t val, e_role role) {
	//return PutINGate(nvals, &val, role);
	uint32_t gateid = PutSIMDINGate(nvals, role);
	if (role == m_eMyRole) {
		//assign value
		GATE* gate = m_pGates + gateid;
		gate->gs.ishare.inval = (UGATE_T*) calloc(ceil_divide(nvals * m_nShareBitLen, sizeof(UGATE_T) * 8), sizeof(UGATE_T));
		memcpy(gate->gs.ishare.inval, &val, ceil_divide(nvals * m_nShareBitLen, 8));

		gate->instantiated = true;
	}

	return gateid;
}

uint32_t BooleanCircuit::PutSharedSIMDINGate(uint32_t nvals, uint64_t val) {
	//return PutINGate(nvals, &val, role);
	uint32_t gateid = PutSharedSIMDINGate(nvals);

	//assign value
	GATE* gate = m_pGates + gateid;
	gate->gs.val = (UGATE_T*) calloc(ceil_divide(nvals * m_nShareBitLen, sizeof(UGATE_T) * 8), sizeof(UGATE_T));
	memcpy(gate->gs.val, &val, ceil_divide(nvals * m_nShareBitLen, 8));

	gate->instantiated = true;
	return gateid;
}


/**
 * When inputting shares with bitlen>1 and nvals convert to regular SIMD in gates
 */
template<class T> share* BooleanCircuit::InternalPutINGate(uint32_t nvals, T val, uint32_t bitlen, e_role role) {
	share* shr = new boolshare(bitlen, this);
	assert(nvals <= sizeof(T) * 8);
	T mask = 0;

	memset(&mask, 0xFF, ceil_divide(nvals, 8));
	mask = mask >> (PadToMultiple(nvals, 8)-nvals);

	for (uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutSIMDINGate(nvals, (val >> i) & mask, role));
	}
	return shr;
}


/**
 * When inputting shares with bitlen>1 and nvals convert to regular SIMD in gates
 */
template<class T> share* BooleanCircuit::InternalPutSharedINGate(uint32_t nvals, T val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	assert(nvals <= sizeof(T) * 8);
	T mask = 0;

	memset(&mask, 0xFF, ceil_divide(nvals, 8));
	mask = mask >> (PadToMultiple(nvals, 8)-nvals);

	for (uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutSharedSIMDINGate(nvals, (val >> i) & mask));
	}
	return shr;
}

uint32_t BooleanCircuit::PutYaoSharedSIMDINGate(uint32_t nvals, yao_fields keys) {
	uint32_t gateid = PutSharedSIMDINGate(nvals);
	//assign value
	GATE* gate = m_pGates + gateid;
	//TODO: fixed to 128-bit security atm. CHANGE
	uint8_t keybytelen = ceil_divide(128, 8);
	if(m_eMyRole == SERVER) {
		gate->gs.yinput.outKey = (uint8_t*) malloc(keybytelen * nvals);
		memcpy(gate->gs.yinput.outKey, keys.outKey, keybytelen * nvals);
		gate->gs.yinput.pi = (uint8_t*) malloc(nvals);
		memcpy(gate->gs.yinput.pi, keys.pi, nvals);
	} else {
		gate->gs.yval = (uint8_t*) malloc(keybytelen * nvals);
		memcpy(gate->gs.yval, keys.outKey, keybytelen * nvals);
	}

	gate->instantiated = true;
	return gateid;

}

share* BooleanCircuit::PutYaoSharedSIMDINGate(uint32_t nvals, yao_fields* keys, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	for(uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutYaoSharedSIMDINGate(nvals, keys[i]));
	}
	return shr;
}




template<class T> share* BooleanCircuit::InternalPutINGate(uint32_t nvals, T* val, uint32_t bitlen, e_role role) {
	share* shr = new boolshare(bitlen, this);
	uint32_t typebitlen = sizeof(T) * 8;
	uint32_t typebyteiters = ceil_divide(bitlen, typebitlen);
	uint64_t tmpval_bytes = max(typebyteiters * nvals, (uint32_t) sizeof(T));// * sizeof(T);
	//uint32_t valstartpos = ceil_divide(nvals, typebitlen);
	T* tmpval = (T*) malloc(tmpval_bytes);

	for (uint32_t i = 0; i < bitlen; i++) {
		memset(tmpval, 0, tmpval_bytes);
		for (uint32_t j = 0; j < nvals; j++) {
			//tmpval[j / typebitlen] += ((val[j] >> (i % typebitlen) & 0x01) << j);
			tmpval[j /typebitlen] += (((val[j * typebyteiters + i/typebitlen] >> (i % typebitlen)) & 0x01) << (j%typebitlen));
		}
		shr->set_wire_id(i, PutSIMDINGate(nvals, tmpval, role));
	}
	free(tmpval);
	return shr;
}

template<class T> share* BooleanCircuit::InternalPutSharedINGate(uint32_t nvals, T* val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	uint32_t typebitlen = sizeof(T) * 8;
	uint32_t typebyteiters = ceil_divide(bitlen, typebitlen);
	uint64_t tmpval_bytes = max(typebyteiters * nvals, (uint32_t) sizeof(T));;// * sizeof(T);
	//uint32_t valstartpos = ceil_divide(nvals, typebitlen);
	T* tmpval = (T*) malloc(tmpval_bytes);

	for (uint32_t i = 0; i < bitlen; i++) {
		memset(tmpval, 0, tmpval_bytes);
		for (uint32_t j = 0; j < nvals; j++) {
			//tmpval[j / typebitlen] += ((val[j] >> (i % typebitlen) & 0x01) << j);
			tmpval[j /typebitlen] += (((val[j * typebyteiters + i/typebitlen] >> (i % typebitlen)) & 0x01) << (j%typebitlen));
		}
		shr->set_wire_id(i, PutSharedSIMDINGate(nvals, tmpval));
	}
	free(tmpval);
	return shr;
}


uint32_t BooleanCircuit::PutOUTGate(uint32_t parentid, e_role dst) {
	uint32_t gateid = m_cCircuit->PutOUTGate(parentid, dst, m_nRoundsOUT[dst]);

	UpdateInteractiveQueue(gateid);

	switch (dst) {
	case SERVER:
		m_vOutputGates[0].push_back(gateid);
		m_vOutputBits[0] += m_pGates[gateid].nvals;
		break;
	case CLIENT:
		m_vOutputGates[1].push_back(gateid);
		m_vOutputBits[1] += m_pGates[gateid].nvals;
		break;
	case ALL:
		m_vOutputGates[0].push_back(gateid);
		m_vOutputGates[1].push_back(gateid);
		m_vOutputBits[0] += m_pGates[gateid].nvals;
		m_vOutputBits[1] += m_pGates[gateid].nvals;
		break;
	default:
		cerr << "Role not recognized" << endl;
		break;
	}

	return gateid;
}

share* BooleanCircuit::PutOUTGate(share* parent, e_role dst) {
	return new boolshare(PutOUTGate(parent->get_wires(), dst), this);
}

vector<uint32_t> BooleanCircuit::PutOUTGate(vector<uint32_t> parentids, e_role dst) {
	vector<uint32_t> gateid = m_cCircuit->PutOUTGate(parentids, dst, m_nRoundsOUT[dst]);

	//TODO: optimize
	for (uint32_t i = 0; i < gateid.size(); i++) {
		UpdateInteractiveQueue(gateid[i]);
		switch (dst) {
		case SERVER:
			m_vOutputGates[0].push_back(gateid[i]);
			m_vOutputBits[0] += m_pGates[gateid[i]].nvals;
			break;
		case CLIENT:
			m_vOutputGates[1].push_back(gateid[i]);
			m_vOutputBits[1] += m_pGates[gateid[i]].nvals;
			break;
		case ALL:
			m_vOutputGates[0].push_back(gateid[i]);
			m_vOutputGates[1].push_back(gateid[i]);
			m_vOutputBits[0] += m_pGates[gateid[i]].nvals;
			m_vOutputBits[1] += m_pGates[gateid[i]].nvals;
			break;
		default:
			cerr << "Role not recognized" << endl;
			break;
		}
	}

	return gateid;
}


vector<uint32_t> BooleanCircuit::PutSharedOUTGate(vector<uint32_t> parentids) {
	vector<uint32_t> out = m_cCircuit->PutSharedOUTGate(parentids);
	for(uint32_t i = 0; i < out.size(); i++) {
		UpdateLocalQueue(out[i]);
	}
	return out;
}

share* BooleanCircuit::PutSharedOUTGate(share* parent) {
	return new boolshare(PutSharedOUTGate(parent->get_wires()), this);
}

share* BooleanCircuit::PutCONSGate(UGATE_T val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	UGATE_T tmpval;
	for (uint32_t i = 0; i < bitlen; i++) {
		(val>>i) & 0x01 ? tmpval = ~0: tmpval = 0;
		tmpval = tmpval % (1<<1);
		shr->set_wire_id(i, PutConstantGate(tmpval, 1));
	}
	return shr;
}

share* BooleanCircuit::PutCONSGate(uint8_t* val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	uint32_t bytelen = ceil_divide(bitlen, 8);
	uint32_t valbytelen = ceil_divide(1, 8);
	uint8_t* tmpval = (uint8_t*) malloc(valbytelen);

	for (uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutConstantGate(val[i] & 0x01, 1));
	}
	free(tmpval);
	return shr;
}

share* BooleanCircuit::PutCONSGate(uint32_t* val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	for (uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutConstantGate((val[i >> 5] >> i) & 0x01, 1));
	}
	return shr;
}

share* BooleanCircuit::PutSIMDCONSGate(uint32_t nvals, UGATE_T val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	uint32_t ugate_iters = ceil_divide(nvals, sizeof(UGATE_T) * 8);
	UGATE_T *tmparray = (UGATE_T*) calloc(ugate_iters, sizeof(UGATE_T));
	UGATE_T tmpval;
	uint32_t j;
	for (uint32_t i = 0; i < bitlen; i++) {
		(val>>i) & 0x01 ? tmpval = ~(0L): tmpval = 0L;
		for(j = 0; j < ugate_iters-1; j++) {
			tmparray[j] = tmpval;
		}
		tmparray[j] = tmpval & (((1L)<<(nvals%64))-1L);
		shr->set_wire_id(i, PutConstantGate(tmpval, nvals));
	}
	free(tmparray);
	return shr;
}

share* BooleanCircuit::PutSIMDCONSGate(uint32_t nvals, uint8_t* val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	uint32_t bytelen = ceil_divide(bitlen, 8);
	uint32_t valbytelen = ceil_divide(nvals, 8);
	uint8_t* tmpval = (uint8_t*) malloc(valbytelen);

	for (uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutConstantGate(val[i] & 0x01, nvals));
	}
	free(tmpval);
	return shr;
}

share* BooleanCircuit::PutSIMDCONSGate(uint32_t nvals, uint32_t* val, uint32_t bitlen) {
	share* shr = new boolshare(bitlen, this);
	for (uint32_t i = 0; i < bitlen; i++) {
		shr->set_wire_id(i, PutConstantGate((val[i >> 5] >> i) & 0x01, nvals));
	}
	return shr;
}


uint32_t BooleanCircuit::PutConstantGate(UGATE_T val, uint32_t nvals) {
	uint32_t gateid = m_cCircuit->PutConstantGate(m_eContext, val, nvals, m_nShareBitLen);
	UpdateLocalQueue(gateid);
	return gateid;
}

uint32_t BooleanCircuit::PutINVGate(uint32_t parentid) {
	uint32_t gateid = m_cCircuit->PutINVGate(parentid);
	UpdateLocalQueue(gateid);
	return gateid;
}

vector<uint32_t> BooleanCircuit::PutINVGate(vector<uint32_t> parentid) {
	vector<uint32_t> out(parentid.size());
	for (uint32_t i = 0; i < out.size(); i++)
		out[i] = PutINVGate(parentid[i]);
	return out;
}

share* BooleanCircuit::PutINVGate(share* parent) {
	return new boolshare(PutINVGate(parent->get_wires()), this);
}

uint32_t BooleanCircuit::PutY2BCONVGate(uint32_t parentid) {
	vector<uint32_t> in(1, parentid);
	uint32_t gateid = m_cCircuit->PutCONVGate(in, 1, S_BOOL, m_nShareBitLen);
	m_pGates[gateid].depth++;
	UpdateLocalQueue(gateid);
	//a Y input gate cannot be parent to a Y2B gate. Alternatively, put a Boolean input gate
	assert(m_pGates[parentid].type != G_IN);

	return gateid;
}

uint32_t BooleanCircuit::PutB2YCONVGate(uint32_t parentid) {
	vector<uint32_t> in(1, parentid);
	uint32_t gateid = m_cCircuit->PutCONVGate(in, 2, S_YAO, m_nShareBitLen);
	UpdateInteractiveQueue(gateid);

	//treat similar to input gate of client and server
	m_nB2YGates += m_pGates[gateid].nvals;

	return gateid;
}

uint32_t BooleanCircuit::PutYSwitchRolesGate(uint32_t parentid) {
	vector<uint32_t> in(1, parentid);
	assert(m_eContext == S_YAO || m_eContext == S_YAO_REV);
	assert(m_pGates[in[0]].context != m_eContext);
	uint32_t gateid = m_cCircuit->PutCONVGate(in, 2, m_eContext, m_nShareBitLen);
	UpdateInteractiveQueue(gateid);

	//treat similar to input gate of client and server
	m_nYSwitchGates += m_pGates[gateid].nvals;

	return gateid;
}

vector<uint32_t> BooleanCircuit::PutYSwitchRolesGate(vector<uint32_t> parentid) {
	vector<uint32_t> out(parentid.size());
	for (uint32_t i = 0; i < parentid.size(); i++) {
		out[i] = PutYSwitchRolesGate(parentid[i]);
	}

	return out;
}


vector<uint32_t> BooleanCircuit::PutY2BCONVGate(vector<uint32_t> parentid) {
	vector<uint32_t> out(parentid.size());
	for (uint32_t i = 0; i < parentid.size(); i++) {
		out[i] = PutY2BCONVGate(parentid[i]);
	}
	return out;
}

vector<uint32_t> BooleanCircuit::PutB2YCONVGate(vector<uint32_t> parentid) {
	vector<uint32_t> out(parentid.size());
	for (uint32_t i = 0; i < parentid.size(); i++) {
		out[i] = PutB2YCONVGate(parentid[i]);
	}

	return out;
}

share* BooleanCircuit::PutY2BGate(share* ina) {
	return new boolshare(PutY2BCONVGate(ina->get_wires()), this);
}

share* BooleanCircuit::PutB2YGate(share* ina) {
	return new boolshare(PutB2YCONVGate(ina->get_wires()), this);
}

share* BooleanCircuit::PutYSwitchRolesGate(share* ina) {
	return new boolshare(PutYSwitchRolesGate(ina->get_wires()), this);
}

vector<uint32_t> BooleanCircuit::PutA2YCONVGate(vector<uint32_t> parentid) {
	vector<uint32_t> srvshares(m_pGates[parentid[0]].sharebitlen);
	vector<uint32_t> clishares(m_pGates[parentid[0]].sharebitlen);

	for (uint32_t i = 0; i < m_pGates[parentid[0]].sharebitlen; i++) {
		srvshares[i] = m_cCircuit->PutCONVGate(parentid, 1, S_YAO, m_nShareBitLen);
		m_pGates[srvshares[i]].gs.pos = 2 * i;
		m_pGates[srvshares[i]].depth++; //increase depth by 1 since yao is evaluated before arith
		UpdateInteractiveQueue(srvshares[i]);

		clishares[i] = m_cCircuit->PutCONVGate(parentid, 2, S_YAO, m_nShareBitLen);
		m_pGates[clishares[i]].gs.pos = 2 * i + 1;
		m_pGates[clishares[i]].depth++; //increase depth by 1 since yao is evaluated before arith
		UpdateInteractiveQueue(clishares[i]);
	}

	m_nA2YGates += m_pGates[parentid[0]].nvals * m_pGates[parentid[0]].sharebitlen;


	return PutAddGate(srvshares, clishares);
}

share* BooleanCircuit::PutA2YGate(share* ina) {
	return new boolshare(PutA2YCONVGate(ina->get_wires()), this);
}

uint32_t BooleanCircuit::PutStructurizedCombinerGate(vector<uint32_t> input, uint32_t pos_start,
		uint32_t pos_incr, uint32_t nvals) {
	uint32_t gateid = m_cCircuit->PutStructurizedCombinerGate(input, pos_start, pos_incr, nvals);
	UpdateLocalQueue(gateid);
	return gateid;
}

share* BooleanCircuit::PutStructurizedCombinerGate(share* input, uint32_t pos_start,
		uint32_t pos_incr, uint32_t nvals) {
	share* out= new boolshare(1, this);
	nstructcombgates++;
	out->set_wire_id(0, PutStructurizedCombinerGate(input->get_wires(), pos_start, pos_incr, nvals));
	return out;
}

uint32_t BooleanCircuit::PutCallbackGate(vector<uint32_t> in, uint32_t rounds, void (*callback)(GATE*, void*),
		void* infos, uint32_t nvals) {

	uint32_t gateid = m_cCircuit->PutCallbackGate(in, rounds, callback, infos, nvals);

	if(rounds > 0) {
		UpdateInteractiveQueue(gateid);
	} else {
		UpdateLocalQueue(gateid);
	}
	return gateid;
}

share* BooleanCircuit::PutCallbackGate(share* in, uint32_t rounds, void (*callback)(GATE*, void*),
		void* infos, uint32_t nvals) {
	return new boolshare(PutCallbackGate(in->get_wires(), rounds, callback, infos, nvals), this);
}

/*uint64_t* transposeTT(uint32_t dima, uint32_t dimb, uint64_t* ttable) {
	uint32_t longbits = sizeof(uint64_t) * 8;
	uint64_t* newtt = (uint64_t*) calloc(bits_in_bytes(dima * dimb), sizeof(uint8_t));

	cout << "dima = " << dima << ", dimb = " << dimb << endl;
	cout << "Before Transposing: " << (hex) << endl;

	for(uint32_t i = 0; i < ceil_divide(dima * dimb, longbits); i++) {
		cout << ttable[i] << " ";
	}
	cout << (dec) << endl;

	for(uint32_t i = 0; i < dima; i++) {
		for(uint32_t j = 0; j < dimb; j++) {
			uint32_t idxsrc = (i * dimb + j);
			uint32_t idxdst = (j * dima  + i);
			newtt[idxdst / longbits] |= (((ttable[idxsrc / longbits] >> (idxsrc % longbits)) & 0x01) << (idxdst % longbits));
		}
	}
	cout << "After Transposing: " << (hex) << endl;
	for(uint32_t i = 0; i < ceil_divide(dima * dimb, longbits); i++) {
		cout << newtt[i] << " ";
	}
	cout << (dec) << endl;

	return newtt;
}*/

vector<uint32_t> BooleanCircuit::PutTruthTableMultiOutputGate(vector<uint32_t> in, uint32_t out_bits,
		uint64_t* ttable) {
	//assert(m_eContext == S_BOOL_NO_MT);

	//uint32_t tmpgate = m_cCircuit->PutTruthTableGate(in, 1, out_bits, ttable);
	//UpdateInteractiveQueue(tmpgate);

	//Transpose truth table
	//ttable = transposeTT(1<<in.size(), out_bits, ttable);
	uint32_t tmpgate = PutTruthTableGate(in, out_bits, ttable);
	vector<uint32_t> bitlens(out_bits, m_pGates[in[0]].nvals);
	assert(out_bits <= 8);

	vector<uint32_t> output = m_cCircuit->PutSplitterGate(tmpgate, bitlens);
	for(uint32_t i = 0; i < output.size(); i++) {
		UpdateLocalQueue(output[i]);
	}

	return output;
}

share* BooleanCircuit::PutTruthTableMultiOutputGate(share* in, uint32_t output_bitlen, uint64_t* ttable) {
	return new boolshare(PutTruthTableMultiOutputGate(in->get_wires(), output_bitlen, ttable), this);
}

uint32_t BooleanCircuit::PutTruthTableGate(vector<uint32_t> in, uint32_t out_bits, uint64_t* ttable) {

	assert(m_eContext == S_BOOL_NO_MT);
	uint32_t gateid = m_cCircuit->PutTruthTableGate(in, 1, out_bits, ttable);
	UpdateTruthTableSizes(1<<in.size(), m_pGates[gateid].nvals/out_bits, m_pGates[gateid].depth, out_bits);

	UpdateInteractiveQueue(gateid);

	return gateid;
}


share* BooleanCircuit::PutTruthTableGate(share* in, uint64_t* ttable) {
	boolshare* out = new boolshare(1, this);
	out->set_wire_id(0, PutTruthTableGate(in->get_wires(), 1, ttable));
	return out;
}

//check if the len exists, otherwise allocate new and update
void BooleanCircuit::UpdateTruthTableSizes(uint32_t len, uint32_t nvals, uint32_t depth, uint32_t out_bits) {
	//check depth and resize if required
	if(depth >= m_vTTlens.size()) {
		uint32_t old_depth = m_vTTlens.size();
		uint32_t nlens = m_vTTlens[0].size();
		m_vTTlens.resize(depth+1);
		//copy old values from 0-pos
		for(uint32_t i = old_depth; i < m_vTTlens.size(); i++) {
			m_vTTlens[i].resize(nlens);
			for(uint32_t j = 0; j < nlens; j++) {
				uint32_t nouts = m_vTTlens[0][j].size();
				m_vTTlens[i][j].resize(nouts);
				for(uint32_t k = 0; k < nouts; k++) {
					m_vTTlens[i][j][k].numgates = 0;
					m_vTTlens[i][j][k].tt_len = m_vTTlens[0][j][k].tt_len;
					m_vTTlens[i][j][k].out_bits = m_vTTlens[0][j][k].out_bits;
				}

			}
		}
	}

	//check whether the address for the input sizes already exist
	bool ins_exist = false;
	bool outs_exist = false;
	uint32_t id;
	for(uint32_t i = 0; i < m_vTTlens[0].size() && !ins_exist; i++) {
		if(len == m_vTTlens[depth][i][0].tt_len) {
			//check whether the bitlen already exists for the input size
			ins_exist = true;
			id = i;
			for(uint32_t j = 0; j < m_vTTlens[depth][i].size() && !outs_exist; j++) {
				if(m_vTTlens[depth][i][j].out_bits == out_bits) {
					outs_exist = true;
					m_vTTlens[depth][i][j].numgates += nvals;
				}
			}
		}
	}
	//the input size does not exist, create new one!
	if(!ins_exist) {
		uint32_t old_in_lens = m_vTTlens[0].size();
		for(uint32_t i = 0; i < m_vTTlens.size(); i++) {
			m_vTTlens[i].resize(old_in_lens+1);
			m_vTTlens[i][old_in_lens].resize(1);
			m_vTTlens[i][old_in_lens][0].tt_len = len;
			m_vTTlens[i][old_in_lens][0].numgates = 0;
			m_vTTlens[i][old_in_lens][0].out_bits = out_bits;
		}
		//m_vTTlens[depth][old_lens].tt_len = len;//should work without this too
		m_vTTlens[depth][old_in_lens][0].numgates = nvals;
		outs_exist = true;
	}

	//the out size do not exist; create new
	if(!outs_exist) {
		uint32_t old_out_lens = m_vTTlens[0][id].size();
		for(uint32_t i = 0; i < m_vTTlens.size(); i++) {
			m_vTTlens[i][id].resize(old_out_lens+1);
			m_vTTlens[i][id][old_out_lens].tt_len = len;
			m_vTTlens[i][id][old_out_lens].numgates = 0;
			m_vTTlens[i][id][old_out_lens].out_bits = out_bits;
		}
		//m_vTTlens[depth][id][old_out_lens].tt_len = len;//should work without this too
		m_vTTlens[depth][id][old_out_lens].numgates = nvals;
		outs_exist = true;
	}
}


//enqueue interactive gate queue
void BooleanCircuit::UpdateInteractiveQueue(uint32_t gateid) {
	if (m_pGates[gateid].depth + 1 > m_vInteractiveQueueOnLvl.size()) {
		m_vInteractiveQueueOnLvl.resize(m_pGates[gateid].depth + 1);
		if (m_pGates[gateid].depth + 1 > m_nMaxDepth) {
			m_nMaxDepth = m_pGates[gateid].depth + 1;
		}
	}
	m_vInteractiveQueueOnLvl[m_pGates[gateid].depth].push_back(gateid);
	m_nGates++;
}

//enqueue locally evaluated gate queue
void BooleanCircuit::UpdateLocalQueue(uint32_t gateid) {
	if (m_pGates[gateid].depth + 1 > m_vLocalQueueOnLvl.size()) {
		//cout << "increasing size of local queue" << endl;
		m_vLocalQueueOnLvl.resize(m_pGates[gateid].depth + 1);
		if (m_pGates[gateid].depth + 1 > m_nMaxDepth) {
			m_nMaxDepth = m_pGates[gateid].depth + 1;
		}
	}
	m_vLocalQueueOnLvl[m_pGates[gateid].depth].push_back(gateid);

	m_nGates++;
}


//shift val by pos positions to the left and fill with zeros
vector<uint32_t> BooleanCircuit::LShift(vector<uint32_t> val, uint32_t pos, uint32_t nvals) {
	vector<uint32_t> out(val.size());
	uint32_t i, zerogate = PutConstantGate(0, nvals);
	for (i = 0; i < pos && i < val.size(); i++) {
		out[i] = zerogate;
	}
	for (i = pos; i < val.size(); i++) {
		out[i] = val[i - pos];
	}
	return out;
}

share* BooleanCircuit::PutADDGate(share* ina, share* inb) {
	//also output the carry of the result as long as the additional carry does not exceed the maximum bit length of the higher of both inputs
	bool carry = max(ina->get_bitlength(), inb->get_bitlength()) < max(ina->get_max_bitlength(), inb->get_max_bitlength());
	return new boolshare(PutAddGate(ina->get_wires(), inb->get_wires(), carry), this);
}


vector<uint32_t> BooleanCircuit::PutAddGate(vector<uint32_t> left, vector<uint32_t> right, BOOL bCarry) {
	PadWithLeadingZeros(left, right);
	if (m_eContext == S_BOOL) {
		return PutDepthOptimizedAddGate(left, right, bCarry);
	} if (m_eContext == S_BOOL_NO_MT) {
		return PutLUTAddGate(left, right, bCarry);
	} else {
		return PutSizeOptimizedAddGate(left, right, bCarry);
	}
}



//a + b, do we need a carry?
vector<uint32_t> BooleanCircuit::PutSizeOptimizedAddGate(vector<uint32_t> a, vector<uint32_t> b, BOOL bCarry) {
	// left + right mod (2^Rep)
	// Construct C[i] gates
	PadWithLeadingZeros(a, b);
	uint32_t rep = a.size();// + (!!bCarry);
	vector<uint32_t> C(rep);
	uint32_t axc, bxc, acNbc;

	C[0] = PutXORGate(a[0], a[0]);//PutConstantGate(0, m_pGates[a[0]].nvals); //the second parameter stands for the number of vals

	uint32_t i = 0;
	for (; i < rep - 1; i++) {
		//===================
		// New Gates
		// a[i] xor c[i]
		axc = PutXORGate(a[i], C[i]);

		// b[i] xor c[i]
		bxc = PutXORGate(b[i], C[i]);

		// axc AND bxc
		acNbc = PutANDGate(axc, bxc);

		// C[i+1]
		C[i + 1] = PutXORGate(C[i], acNbc);
	}

#ifdef ZDEBUG
	cout << "Finished carry generation" << endl;
#endif

	if (bCarry) {
		axc = PutXORGate(a[i], C[i]);

		// b[i] xor c[i]
		bxc = PutXORGate(b[i], C[i]);

		// axc AND bxc
		acNbc = PutANDGate(axc, bxc);
	}

#ifdef ZDEBUG
	cout << "Finished additional carry generation" << endl;
#endif

	// Construct a[i] xor b[i] gates
	vector<uint32_t> AxB(rep);
	for (uint32_t i = 0; i < rep; i++) {
		// a[i] xor b[i]
		AxB[i] = PutXORGate(a[i], b[i]);
	}

#ifdef ZDEBUG
	cout << "Finished parity on inputs" << endl;
#endif

	// Construct Output gates of Addition
	vector<uint32_t> out(rep + (!!bCarry));
	for (uint32_t i = 0; i < rep; i++) {
		out[i] = PutXORGate(C[i], AxB[i]);
	}

#ifdef ZDEBUG
	cout << "Finished parity on inputs xor carries" << endl;
#endif

	if (bCarry)
		out[rep] = PutXORGate(C[i], acNbc);

#ifdef ZDEBUG
	cout << "Finished parity on additional carry and inputs" << endl;
#endif

	return out;
}



//TODO: there is a bug when adding 3 and 1 as two 2-bit numbers and expecting a carry
vector<uint32_t> BooleanCircuit::PutDepthOptimizedAddGate(vector<uint32_t> a, vector<uint32_t> b, BOOL bCARRY, bool vector_and) {
	PadWithLeadingZeros(a, b);
	uint32_t id, rep = min(a.size(), b.size());
	vector<uint32_t> out(a.size() + bCARRY);
	vector<uint32_t> parity(a.size()), carry(rep), parity_zero(rep);
	uint32_t zerogate = PutConstantGate(0, m_pGates[a[0]].nvals);
	uint32_t startid = zerogate;
	share* zero_share = new boolshare(2, this);
	share* ina = new boolshare(2, this);
	share* sel = new boolshare(1, this);
	share* s_out = new boolshare(2, this);
	zero_share->set_wire_id(0, zerogate);
	zero_share->set_wire_id(1, zerogate);

	for (uint32_t i = 0; i < rep; i++) { //0-th layer
		parity[i] = PutXORGate(a[i], b[i]);
		parity_zero[i] = parity[i];
		carry[i] = PutANDGate(a[i], b[i]);
	}

	for (uint32_t i = 1; i <= (uint32_t) ceil(log(rep) / log(2)); i++) {
		for (uint32_t j = 0; j < rep; j++) {
			if (j % (uint32_t) pow(2, i) >= pow(2, (i - 1))) {
				id = pow(2, (i - 1)) + pow(2, i) * ((uint32_t) floor(j / (pow(2, i)))) - 1;
				if(m_eContext == S_BOOL && vector_and) {
					ina->set_wire_id(0, carry[id]);
					ina->set_wire_id(1, parity[id]);
					sel->set_wire_id(0, parity[j]);
					PutMultiMUXGate(&ina, &zero_share, sel, 1, &s_out);
					//carry[j] = PutINVGate(PutANDGate(PutINVGate(s_out->get_wire(0)), PutINVGate(carry[j])));
					carry[j] = PutXORGate(s_out->get_wire_id(0), carry[j]);
					parity[j] = s_out->get_wire_id(1);
				} else {
					//carry[j] = PutINVGate(PutANDGate(PutINVGate(PutANDGate(parity[j], carry[id])), PutINVGate(carry[j]))); // c = (p and c-1) or c = (((p and c-1) xor 1) and (c xor 1)) xor 1)
					carry[j] = PutXORGate(carry[j], PutANDGate(parity[j], carry[id])); // c = c XOR (p and c-1), from ShallowCC
					parity[j] = PutANDGate(parity[j], parity[id]);
				}
			}
		}
	}
	out[0] = parity_zero[0];
	for (uint32_t i = 1; i < rep; i++) {
		out[i] = PutXORGate(parity_zero[i], carry[i - 1]);
	}
	if (bCARRY)	//Do I expect a carry in the most significant bit position?
		out[rep] = carry[rep - 1];

	delete zero_share;
	delete ina;
	delete sel;
	delete s_out;
	return out;
}


// A carry-save adder
vector<vector<uint32_t> > BooleanCircuit::PutCarrySaveGate(vector<uint32_t> a, vector<uint32_t> b, vector<uint32_t> c, uint32_t inbitlen, bool carry) {
	vector<uint32_t> axc(inbitlen);
	vector<uint32_t> acNbc(inbitlen);
	vector<vector<uint32_t> > out(2);

	/*PutPrintValueGate(new boolshare(a, this), "Carry Input A");
	PutPrintValueGate(new boolshare(b, this), "Carry Input B");
	PutPrintValueGate(new boolshare(c, this), "Carry Input C");*/

	for (uint32_t i = 0; i < inbitlen; i++) {
		axc[i] = PutXORGate(a[i],c[i]); //i*3 - 2
		acNbc[i] = PutANDGate(axc[i], PutXORGate(b[i],c[i])); //2+i*3
	}

	if(carry) {
		out[0].resize(inbitlen+1);
		out[0][inbitlen] = PutConstantGate(0, GetNumVals(out[0][0]));
		out[1].resize(inbitlen+1);
		out[1][inbitlen] = PutXORGate(acNbc[inbitlen-1],c[inbitlen-1]);
	} else {
		out[0].resize(inbitlen);
		out[1].resize(inbitlen);
	}

	for (uint32_t i = 0; i < inbitlen; i++) {
		out[0][i] = PutXORGate(b[i],axc[i]);
	}

	out[1][0] = PutConstantGate(0, GetNumVals(out[0][0]));
	for (uint32_t i = 0; i < inbitlen-1; i++) {
		out[1][i+1] = PutXORGate(acNbc[i],c[i]);
	}

	/*PutPrintValueGate(new boolshare(out[0], this), "Carry Output 0");
	PutPrintValueGate(new boolshare(out[1], this), "Carry Output 1");*/

	return out;
}



/*
 * In implementation of the Brent-Kung adder for the Bool-No-MT sharing. To process the values, 5 LUTs are needed:
 * 1) for the inputs, 2) for intermediate carry-forwarding, 3) for critical path on inputs, 4) for the critical path, 5) for the inverse carry tree.
 */
vector<uint32_t> BooleanCircuit::PutLUTAddGate(vector<uint32_t> a, vector<uint32_t> b, BOOL bCARRY) {
	uint32_t id, rep = max(a.size(), b.size());
	PadWithLeadingZeros(a, b);
	vector<uint32_t> out(a.size() + bCARRY);
	vector<uint32_t> parity(rep), carry(rep), parity_zero(rep), tmp;
	vector<uint32_t> lut_in(2*rep);
	uint32_t max_ins = 4, processed_ins;
	uint32_t max_invs = 7;

	uint32_t n_crit_ins = min(rep, (uint32_t) max_ins);
	vector<uint32_t> tmpout;

	//cout << "Building a LUT add gate for " << rep << " input bits" << endl;

	//step 1: process the input values and generate carry / parity signals
	//compute the parity bits for the zero-th layer. Are needed for the result
	for (uint32_t i = 0; i < rep; i++) { //0-th layer
		parity_zero[i] = PutXORGate(a[i], b[i]);
		parity[i] = parity_zero[i];
	}

	lut_in.clear();
	lut_in.resize(n_crit_ins*2);
	for(uint32_t i = 0; i < n_crit_ins; i++) {
		lut_in[2*i] = a[i];
		lut_in[2*i+1] = b[i];
	}
	//process the first bits on the critical path and obtain the carry bits
	//cout << "building a crit path input gate with nins = " << n_crit_ins << endl;
	tmp = PutTruthTableMultiOutputGate(lut_in, n_crit_ins, (uint64_t*) m_vLUT_ADD_CRIT_IN[n_crit_ins-1]);
	for(uint32_t i = 0; i < tmp.size(); i++) {
		carry[i] = tmp[i];
	}

	//process the remaining input bits to have all carry / parity signals
	for(uint32_t i = n_crit_ins; i < rep; ) {
		processed_ins = min(rep - i, max_ins);
		//assign values to the LUT
		lut_in.clear();
		lut_in.resize(2*processed_ins);

		//cout << "building a standard input gate with nins = " << processed_ins << ", i = " << i << " and first val = " << (hex) << m_vLUT_ADD_IN[processed_ins-1][0] <<(dec)<<
		//		", lut_in.size() = " << lut_in.size() << ", expecting nouts = " << m_vLUT_ADD_N_OUTS[processed_ins-1] << endl;
		//cout << "Inputs: ";
		for(uint32_t j = 0; j < processed_ins; j++) {
			lut_in[2*j] = a[i+j];
			lut_in[2*j+1] = b[i+j];
			//cout << i + j << ", ";
		}
		//process inputs via LUT and write updated gates into carry / parity vectors
		tmp = PutTruthTableMultiOutputGate(lut_in, m_vLUT_ADD_N_OUTS[processed_ins-1], (uint64_t*) m_vLUT_ADD_IN[processed_ins-1]);
		//cout << ", outputs = " << i;
		carry[i] = tmp[0];
		if(processed_ins > 1) {
			//cout << ", " << i+1;
			parity[i+1] = tmp[1];
			carry[i+1] = tmp[2];
			if(processed_ins > 2) {
				//cout << ", " << i+2;
				carry[i+2] = tmp[3];
				if(processed_ins > 3) {
					//cout << ", " << i+3;
					parity[i+3] = tmp[4];
					carry[i+3] = tmp[5];
				}
			}
		}
		//cout << endl;
		i+= processed_ins;
	}

	//step 2: process the carry / parity signals and forward them in the tree
	for(uint32_t d = 1; d < ceil_log2(rep+1)/2; d++) {
		//step 2.1: process the carry signals on the critical path
		uint32_t base = 8 * (1<<(2*(d-1)));
		uint32_t dist = base/2;

		processed_ins = 1+ min((rep - base)/dist, max_ins-2);
		//cout << "critical intermediate base = " << base << ", dist = " << dist << ", processed_ins = " << processed_ins << endl;

		lut_in.clear();
		lut_in.resize(2*processed_ins+1);
		lut_in[0] = carry[(base-1)-dist];
		for(uint32_t j = 0; j < processed_ins; j++) {
			lut_in[2*j+1] = parity[(base-1)+j*(dist)];
			lut_in[2*j+2] = carry[(base-1)+j*(dist)];
		}
		//cout << "building a crit-path lut with " << lut_in.size() << " input wires: " << base-1-dist << ", ";
		/*for(uint32_t j = 0; j < processed_ins; j++) {
			cout << base+j*dist << ", ";
		}
		cout << endl;*/
		tmp = PutTruthTableMultiOutputGate(lut_in, processed_ins, (uint64_t*) m_vLUT_ADD_CRIT[processed_ins-1]);
		for(uint32_t j = 0; j < tmp.size(); j++) {
			carry[base-1+j*dist] = tmp[j];
		}

		//step 2.2: forward carry and parity signals down the tree
		for(uint32_t i = (base+3*dist)-1; i+dist < rep; i+=(4*dist)) {
			processed_ins = min(ceil_divide((rep - (i+dist)),2*dist), max_ins-2);
			//cout << "intermediate base = " << i << ", dist = " << dist << ", processed_ins = " << processed_ins << endl;

			lut_in.clear();
			lut_in.resize(4*processed_ins);
			//cout << "building an internal lut with " << lut_in.size() << " input wires: ";

			for(uint32_t j = 0; j < processed_ins*2; j++) {
				lut_in[2*j] = parity[i+j*(dist)];
				lut_in[2*j+1] = carry[i+j*(dist)];
				//cout << i+j*dist << ", ";
			}
			//cout << endl;
			tmp = PutTruthTableMultiOutputGate(lut_in, processed_ins*2, (uint64_t*) m_vLUT_ADD_INTERNAL[processed_ins-1]);
			//cout << "Truth table = " << m_vLUT_ADD_INTERNAL[processed_ins-1][0] << endl;
			for(uint32_t j = 0; j < tmp.size()/2; j++) {
				//cout << "writing bit " << 2*j << " and " << 2*j+1 << " to parity/carry position " << i+dist+2*j*dist << endl;
				parity[i+dist+j*2*dist] = tmp[2*j];
				carry[i+dist+j*2*dist] = tmp[2*j+1];
			}
		}

	}

	//cout << "Doing " << (floor_log2(rep/5)/2)+1 << " iterations on the inverse carry tree, " << floor_log2(rep/5) << ", " << rep/5 << endl;
	//step 3: build the inverse carry tree
	//d increases with d = 0: 5, d = 1: 20, d = 2: 80; d = 3: 320, ...
	for(int32_t d = (floor_log2(rep/5)/2); d >= 0; d--) {
		//for d = 0: 4, for d = 1: 16, for d = 2: 64
		uint32_t start = 4 * (1<<(2*d));
		//for start = 4: 1, start = 16: 4, start = 64: 16
		uint32_t dist = start/4;
		//cout << "d = " << d << ", start = " << start << ", dist = " << dist << endl;
		for(uint32_t i = start; i < rep; i+=start) {
			//processed_ins here needs to be between 1 and 3
			//processed_ins = min(rep - i, max_ins-1);
			processed_ins = min((rep - i)/dist, max_ins-1);
			if(processed_ins > 0) {
				//assign values to the LUT
				lut_in.clear();
				lut_in.resize(2*processed_ins+1);
				lut_in[0] = carry[i-1];
				for(uint32_t j = 0; j < processed_ins; j++) {
					lut_in[2*j+1] = parity[(i-1)+(j+1)*dist];
					lut_in[2*j+2] = carry[(i-1)+(j+1)*dist];
				}
				//cout << "wanting to build gate "<< endl;
				//cout << "Building INV gate with " << processed_ins << " inputs: " << i-1;
				/*for(uint32_t j = 0; j < processed_ins; j++) {
					cout << ", " << (i-1)+(j+1)*dist;
				}*/
				//cout << endl;
				tmp = PutTruthTableMultiOutputGate(lut_in, processed_ins, (uint64_t*) m_vLUT_ADD_INV[processed_ins-1]);
				//cout << "done" << endl;
				//copy resulting carry bit into carries
				//cout << ", and " << tmp.size() << " outputs: ";
				for(uint32_t j = 0; j < tmp.size(); j++) {
					carry[(i-1)+(j+1)*dist] = tmp[j];
					//cout << (i-1)+(j+1)*dist << ", ";
				}
				//cout << endl;
			}
			//i+= dist;//(processed_ins+1);
		}
	}


	//step 4: compute the outputs from the carry signals and the parity bits at the zero-th level
	out[0] = parity_zero[0];
	for (uint32_t i = 1; i < rep; i++) {
		out[i] = PutXORGate(parity_zero[i], carry[i - 1]);
	}
	if (bCARRY)	//Do I expect a carry in the most significant bit position?
		out[rep] = carry[rep - 1];

	return out;
}

vector<uint32_t> BooleanCircuit::PutMulGate(vector<uint32_t> a, vector<uint32_t> b, uint32_t resbitlen, bool depth_optimized, bool vector_ands) {
	PadWithLeadingZeros(a, b);
	//cout << "a.size() = " << a.size() << ", b.size() = " << b.size() << endl;
	uint32_t rep = a.size();

	if(rep == 1) {
		return PutANDGate(a, b);
	}

	vector<vector<uint32_t> > vAdds(rep);
	uint32_t zerogate = PutConstantGate(0, m_pGates[a[0]].nvals);

	uint32_t lim = min(resbitlen, 2 * rep);


	if(m_eContext== S_BOOL && vector_ands) {
		share *ina, *inb, **mulout, *zero_share;
		ina = new boolshare(a, this); //ina = (share**) malloc(sizeof(share*) * 1);
		inb = new boolshare(b, this);//inb = (share**) malloc(sizeof(share*) * 1);
		zero_share = new boolshare(rep, this);

		mulout = (share**) malloc(sizeof(share*) * rep);

		for(uint32_t i = 0; i < rep; i++) {
			mulout[i] = new boolshare(rep, this);
			zero_share->set_wire_id(i, zerogate);
		}

		for(uint32_t i = 0; i < rep; i++) {
			PutMultiMUXGate(&ina, &zero_share, inb->get_wire_ids_as_share(i),  1, &(mulout[i]));
		}

		for (uint32_t i = 0, ctr; i < rep; i++) {
			ctr = 0;
			vAdds[i].resize(lim);

			for (uint32_t j = 0; j < i && ctr < lim; j++, ctr++) {
				vAdds[i][ctr] = zerogate;
			}
			for (uint32_t j = 0; j < rep && ctr < lim; j++, ctr++) {
				vAdds[i][ctr] = mulout[j]->get_wire_id(i);//(a[j], b[i]);
			}
			for (uint32_t j = i; j < rep && ctr < lim; j++, ctr++) {
				vAdds[i][ctr] = zerogate;
			}
		}

		free(mulout);
	} else {
	// Compute AND between all bits
#ifdef ZDEBUG
	cout << "Starting to construct multiplication gate for " << rep << " bits" << endl;
#endif

		for (uint32_t i = 0, ctr; i < rep; i++) {
			ctr = 0;
			vAdds[i].resize(lim);
#ifdef ZDEBUG
			cout << "New Iteration with ctr = " << ctr << ", and lim = " << lim << endl;
#endif
			for (uint32_t j = 0; j < i && ctr < lim; j++, ctr++) {
				vAdds[i][ctr] = zerogate;
			}
			for (uint32_t j = 0; j < rep && ctr < lim; j++, ctr++) {
				vAdds[i][ctr] = PutANDGate(a[j], b[i]);
			}
			for (uint32_t j = i; j < rep && ctr < lim; j++, ctr++) {
				vAdds[i][ctr] = zerogate;
			}
		}
	}


	if (depth_optimized) {
		vector<vector<uint32_t> > out = PutCSNNetwork(vAdds);
		return PutDepthOptimizedAddGate(out[0], out[1]);
	} else {
		return PutWideAddGate(vAdds);

	}

}



share* BooleanCircuit::PutMULGate(share* ina, share* inb) {
	//set the resulting bit length to be the smallest of: 1) bit length of the products or 2) the highest maximum bit length between ina and inb
	uint32_t resbitlen = min(ina->get_bitlength() + inb->get_bitlength(), max(ina->get_max_bitlength(), inb->get_max_bitlength()));
	return new boolshare(PutMulGate(ina->get_wires(), inb->get_wires(), resbitlen), this);
}

share* BooleanCircuit::PutGTGate(share* ina, share* inb) {
	share* shr = new boolshare(1, this);
	shr->set_wire_id(0, PutGTGate(ina->get_wires(), inb->get_wires()));
	return shr;
}
share* BooleanCircuit::PutEQGate(share* ina, share* inb) {
	share* shr = new boolshare(1, this);
	shr->set_wire_id(0, PutEQGate(ina->get_wires(), inb->get_wires()));
	return shr;
}
share* BooleanCircuit::PutMUXGate(share* ina, share* inb, share* sel) {
	return new boolshare(PutMUXGate(ina->get_wires(), inb->get_wires(), sel->get_wire_id(0)), this);
}

vector<uint32_t> BooleanCircuit::PutWideAddGate(vector<vector<uint32_t> > ins) {
	// build a balanced binary tree
	vector<vector<uint32_t> >& survivors = ins;

	while (survivors.size() > 1) {
		unsigned j = 0;
		for (unsigned i = 0; i < survivors.size();) {
			if (i + 1 >= survivors.size()) {
				survivors[j++] = survivors[i++];
			} else {
				survivors[j++] = PutSizeOptimizedAddGate(survivors[i], survivors[i + 1], false);
				i += 2;
			}
		}
		survivors.resize(j);
	}

	return survivors[0];
}

vector<vector<uint32_t> > BooleanCircuit::PutCSNNetwork(vector<vector<uint32_t> > ins) {
	// build a balanced carry-save network
	uint32_t rep = ins[0].size();
	uint32_t wires = ins.size();
	vector<vector<uint32_t> > survivors(wires * 2);// = ins;
	vector<vector<uint32_t> > carry_lines(wires-2);
	vector<vector<uint32_t> > rem(8);
	vector<vector<uint32_t> > out(2);
	int p_head=wires, p_tail = 0, c_head = 0, c_tail = 0, temp_gates;
	vector<uint32_t> dummy(rep);

	for(uint32_t i = 0; i < ins.size(); i++) {
		survivors[i] = ins[i];
	}

	if(ins.size() < 3)
		return ins;

	while(wires > 2) {
		for(; c_tail<c_head-2; c_tail+=3) {
#ifdef ZDEBUG
			cout << "ctail: " << c_tail << ", c_head: " << c_head << endl;
#endif
			//temp_gates = m_nFrontier;
			out = PutCarrySaveGate(carry_lines[c_tail], carry_lines[c_tail+1], carry_lines[c_tail+2], rep);
#ifdef ZDEBUG
	cout << "Computing Carry CSA for gates " << survivors[p_tail] << ", " << survivors[p_tail+1] << ", " << survivors[p_tail+2] << " and bitlen: " << (2*rep-1) << ", gates before: " << temp_gates << ", gates after: " << m_nFrontier << endl;
#endif
			survivors[p_head++] = out[0];
			carry_lines[c_head++] = out[1];
			wires--;
		}
		for(; p_tail<p_head-2; p_tail+=3) {
#ifdef ZDEBUG
			cout << "ptail: " << p_tail << ", p_head: " << p_head << endl;
#endif
			//temp_gates = m_nFrontier;
			out = PutCarrySaveGate(survivors[p_tail], survivors[p_tail+1], survivors[p_tail+2], rep);
#ifdef ZDEBUG
	cout << "Computing Parity CSA for gates " << survivors[p_tail] << ", " << survivors[p_tail+1] << ", " << survivors[p_tail+2] << " and bitlen: " << (2*rep-1) <<  ", gates before: " << temp_gates << ", gates after: " << m_nFrontier << endl;
#endif
			survivors[p_head++] = out[0];
			carry_lines[c_head++] = out[1];
			wires--;
		}
		if((p_head-p_tail) < 3 && (c_head-c_tail) < 3 && wires > 2)	{
#ifdef ZDEBUG
	cout << "Less than 3 in both, Carry and XOR" << endl;
#endif
			uint32_t left = (p_head-p_tail) + (c_head-c_tail);
			rem[0] = survivors[p_tail];
			rem[1] = (p_head-p_tail)>1? survivors[p_tail+1] : carry_lines[c_tail];
			rem[2] = (p_head-p_tail)>1? carry_lines[c_tail] : carry_lines[c_tail+1];
			rem[3] = left > 3? carry_lines[c_tail+1] : dummy;//the dummy value should never be used!
			for(uint32_t j = 0; j < left && wires > 2; j+=3)	{
#ifdef ZDEBUG
				cout << "left: " << left << ", j: " << j << endl;
#endif
				//temp_gates = m_nFrontier;
				out = PutCarrySaveGate(rem[j], rem[j+1], rem[j+2], rep);
#ifdef ZDEBUG
	cout << "Computing Finish CSA for gates " << rem[j] << ", " << rem[j+1] << ", " << rem[j+2] << " and bitlen: " << (2*rep-1) << ", wires: " << wires << ", gates before: " << temp_gates << ", gates after: " << m_nFrontier << endl;
#endif
				rem[left++] = out[0];
				rem[left++] = out[1];
				wires--;
			}
#ifdef ZDEBUG
			cout << "Computed last CSA gate, wires = " << wires << ", ending " << endl;
#endif
		}
	}
#ifdef ZDEBUG
	cout << "Returning" << endl;
#endif
	return out;
}


vector<uint32_t> BooleanCircuit::PutSUBGate(vector<uint32_t> a, vector<uint32_t> b, uint32_t max_bitlength) {
	//pad with leading zeros
	if(a.size() < max_bitlength) {
		uint32_t zerogate = PutConstantGate(0, m_pGates[a[0]].nvals);
		a.resize(max_bitlength, zerogate);
	}
	if(b.size() < max_bitlength) {
		uint32_t zerogate = PutConstantGate(0, m_pGates[a[0]].nvals);
		b.resize(max_bitlength, zerogate);
	}

	uint32_t bitlen = a.size();
	vector<uint32_t> C(bitlen);
	uint32_t i, bc, bxc, ainvNbxc, ainvNbxcObc, axb;
	vector<uint32_t> ainv(bitlen);
	vector<uint32_t> out(bitlen);

	for (i = 0; i < bitlen; i++) {
		ainv[i] = PutINVGate(a[i]);
	}

	C[0] = PutConstantGate(0, m_pGates[a[0]].nvals);

	for (i = 0; i < bitlen - 1; i++) {
		//===================
		// New Gates
		// b[i] and c[i]
		bc = PutANDGate(b[i], C[i]);

		// b[i] xor c[i]
		bxc = PutXORGate(b[i], C[i]);

		ainvNbxc = PutANDGate(ainv[i], bxc);

		// C[i+1] -> (inv(a)AND(b XOR C[i])) OR (b AND C[i])
		C[i + 1] = PutORGate(ainvNbxc, bc);
	}

	for (i = 0; i < bitlen; i++) {
		// a[i] xor b[i]
		axb = PutXORGate(a[i], b[i]);
		out[i] = PutXORGate(axb, C[i]);
	}
	//out = PutDepthOptimizedAddGate(ainv, b);


	return out;
}

share* BooleanCircuit::PutSUBGate(share* ina, share* inb) {
	return new boolshare(PutSUBGate(ina->get_wires(), inb->get_wires(), max(ina->get_max_bitlength(), inb->get_max_bitlength())), this);
}




//computes: ci = a > b ? 1 : 0; but assumes both values to be of equal length!
uint32_t BooleanCircuit::PutGTGate(vector<uint32_t> a, vector<uint32_t> b) {
	PadWithLeadingZeros(a, b);

	if (m_eContext == S_YAO) {
		return PutSizeOptimizedGTGate(a, b);
	} else if(m_eContext == S_BOOL) {
		return PutDepthOptimizedGTGate(a, b);
	} else {
		return PutLUTGTGate(a, b);
	}
}

//computes: ci = a > b ? 1 : 0; but assumes both values to be of equal length!
uint32_t BooleanCircuit::PutSizeOptimizedGTGate(vector<uint32_t> a, vector<uint32_t> b) {
	PadWithLeadingZeros(a, b);
	uint32_t ci = 0, ci1, ac, bc, acNbc;
	ci = PutConstantGate((UGATE_T) 0, m_pGates[a[0]].nvals);
	for (uint32_t i = 0; i < a.size(); i++, ci = ci1) {
		ac = PutXORGate(a[i], ci);
		bc = PutXORGate(b[i], ci);
		acNbc = PutANDGate(ac, bc);
		ci1 = PutXORGate(a[i], acNbc);
	}

	return ci;
}


uint32_t BooleanCircuit::PutDepthOptimizedGTGate(vector<uint32_t> a, vector<uint32_t> b) {
	PadWithLeadingZeros(a, b);
	uint32_t i, rem;
	uint32_t rep = min(a.size(), b.size());
	vector<uint32_t> agtb(rep);
	vector<uint32_t> eq(rep);

	//Put the leaf comparison nodes from which the tree is built
	for (i = 0; i < rep; i++) {
		agtb[i] = PutANDGate(a[i], PutINVGate(b[i])); //PutBitGreaterThanGate(a[i], b[i]);
	}

	//compute the pairwise bit equality from bits 1 to bit rep
	for (i = 1;  i < rep; i++) {
		eq[i] = PutINVGate(PutXORGate(a[i], b[i]));
	}

	rem = rep;

	while (rem > 1) {
		uint32_t j = 0;
		//cout << "New iter with " << size << " element remaining"<< endl;
		for (i = 0; i < rem;) {
			if (i + 1 >= rem) {
				agtb[j] = agtb[i];
				eq[j] = eq[i];
				i++;
				j++;
			} else {
				//cout << j << " = GT" << i+1 << " XOR " << " ( EQ" << i+1 << " AND GT" << i << ")" << endl;
				agtb[j] = PutXORGate(agtb[i+1], PutANDGate(eq[i+1], agtb[i]));
				if(j > 0) {
					eq[j] = PutANDGate(eq[i], eq[i+1]);
				}
				i += 2;
				j++;
			}
		}
		rem = j;
	}


#ifdef ZDEBUG
	cout << "Finished greater than tree with adress: " << agtb[0] << ", and bitlength: " << a.size() << endl;
#endif
	return agtb[0];
}

uint32_t BooleanCircuit::PutLUTGTGate(vector<uint32_t> a, vector<uint32_t> b) {
	// build a balanced 8-wise tree
	uint32_t nins, maxins = 8, minins = 0, j = 0;
	vector<uint32_t> lut_ins, tmp;

	//copy a and b into an internal state
	assert(a.size() == b.size());
	vector<uint32_t> state(a.size() + b.size());
	for(uint32_t i = 0; i < a.size(); i++) {
		state[2*i] = a[i];
		state[2*i+1] = b[i];
	}

	//build the leaf nodes for the tree
	for(uint32_t i = 0; i < state.size(); ) {
		//assign inputs for this node
		nins = min(maxins, (uint32_t) state.size() - i);

		//nins should always be a multiple of two
		assert((nins & 0x01) == 0);
		lut_ins.clear();
		lut_ins.assign(state.begin() + i, state.begin() + i + nins);
		tmp = PutTruthTableMultiOutputGate(lut_ins, 2, (uint64_t*) m_vLUT_GT_IN[(nins/2)-1]);
		//cout << "using lut " << (hex) << m_vLUT_GT_IN[(nins/2)-1][0] << (dec) << endl;

		//assign gt bit and eq bit to state
		state[j] = tmp[0];
		state[j+1] = tmp[1];

		i+=nins;
		j+=2;
	}

	//resize the state since we processed input bits
	state.resize(j);

	//build the tree for the remaining bits
	while (state.size() > 2) {
		j = 0;
		for (uint32_t i = 0; i < state.size();) {
			nins = min(maxins, (uint32_t) state.size()-i);

			//it is not efficient to build a gate here so copy the wires to the next level
			if(nins <= minins && state.size() > minins) {
				for(; i < state.size();) {
					state[j++] = state[i++];
				}
			} else {
				lut_ins.clear();
				lut_ins.assign(state.begin() + i, state.begin() + i + nins);
				tmp = PutTruthTableMultiOutputGate(lut_ins, 2, (uint64_t*) m_vLUT_GT_INTERNAL[nins/2-2]);
				state[j] = tmp[0];
				state[j+1] = tmp[1];

				i += nins;
				j += 2;
			}

		}
		state.resize(j);
	}
	return state[0];
}


uint32_t BooleanCircuit::PutEQGate(vector<uint32_t> a, vector<uint32_t> b) {
	PadWithLeadingZeros(a, b);

	uint32_t rep = a.size(), temp;
	vector<uint32_t> xors(rep);
	for (uint32_t i = 0; i < rep; i++) {
		temp = PutXORGate(a[i], b[i]);
		xors[i] = PutINVGate(temp);
	}

	// AND of all xor's
	if(m_eContext == S_BOOL_NO_MT) {
		return PutLUTWideANDGate(xors);
	} else {
		return PutWideGate(G_NON_LIN, xors);
	}
}



uint32_t BooleanCircuit::PutORGate(uint32_t a, uint32_t b) {
	return PutINVGate(PutANDGate(PutINVGate(a), PutINVGate(b)));
}

vector<uint32_t> BooleanCircuit::PutORGate(vector<uint32_t> a, vector<uint32_t> b) {
	uint32_t reps = max(a.size(), b.size());
	PadWithLeadingZeros(a, b);
	vector<uint32_t> out(reps);
	for (uint32_t i = 0; i < reps; i++) {
		out[i] = PutORGate(a[i], b[i]);
	}
	return out;
}

share* BooleanCircuit::PutORGate(share* a, share* b) {
	return new boolshare(PutORGate(a->get_wires(), b->get_wires()), this);
}

/* if c [0] = s & a[0], c[1] = s & a[1], ...*/
share* BooleanCircuit::PutANDVecGate(share* ina, share* inb) {
	uint32_t rep = ina->get_bitlength();
	share* out = new boolshare(rep, this);

	if (m_eContext == S_BOOL) {
		for (uint32_t i = 0; i < rep; i++) {
			out->set_wire_id(i, PutVectorANDGate(inb->get_wire_id(i), ina->get_wire_id(i)));
		}
	} else {
		//cout << "Putting usual AND gate" << endl;
		for (uint32_t i = 0; i < rep; i++) {
			uint32_t bvec = PutRepeaterGate(inb->get_wire_id(i), m_pGates[ina->get_wire_id(i)].nvals);
			out->set_wire_id(i, PutANDGate(ina->get_wire_id(i), bvec));
		}
	}
	return out;
}

/* if s == 0 ? b : a*/
vector<uint32_t> BooleanCircuit::PutMUXGate(vector<uint32_t> a, vector<uint32_t> b, uint32_t s, BOOL vecand) {
	vector<uint32_t> out;
	uint32_t rep = max(a.size(), b.size());
	uint32_t sab, ab;

	PadWithLeadingZeros(a, b);

	out.resize(rep);

	uint32_t nvals=1;
	for(uint32_t i = 0; i < a.size(); i++) {
		if(m_pGates[a[i]].nvals > nvals)
			nvals = m_pGates[a[i]].nvals;
	}
	for(uint32_t i = 0; i < b.size(); i++)
		if(m_pGates[b[i]].nvals > nvals)
			nvals = m_pGates[b[i]].nvals;

	if (m_eContext == S_BOOL && vecand && nvals == 1) {
		uint32_t avec = PutCombinerGate(a);
		uint32_t bvec = PutCombinerGate(b);

		out = PutSplitterGate(PutVecANDMUXGate(avec, bvec, s));

	} else {
		for (uint32_t i = 0; i < rep; i++) {
			ab = PutXORGate(a[i], b[i]);
			sab = PutANDGate(s, ab);
			out[i] = PutXORGate(b[i], sab);
		}
	}

	return out;
}

share* BooleanCircuit::PutVecANDMUXGate(share* a, share* b, share* s) {
	return new boolshare(PutVecANDMUXGate(a->get_wires(), b->get_wires(), s->get_wires()), this);
}

/* if s == 0 ? b : a*/
vector<uint32_t> BooleanCircuit::PutVecANDMUXGate(vector<uint32_t> a, vector<uint32_t> b, vector<uint32_t> s) {
	uint32_t nmuxes = a.size();
	PadWithLeadingZeros(a, b);

	vector<uint32_t> out(nmuxes);
	uint32_t sab, ab;

	//cout << "Putting Vector AND gate" << endl;

	for (uint32_t i = 0; i < nmuxes; i++) {
		ab = PutXORGate(a[i], b[i]);
		sab = PutVectorANDGate(s[i], ab);
		out[i] = PutXORGate(b[i], sab);
	}

	return out;
}

/* if s == 0 ? b : a*/
uint32_t BooleanCircuit::PutVecANDMUXGate(uint32_t a, uint32_t b, uint32_t s) {
	uint32_t ab, sab;
	ab = PutXORGate(a, b);
	if (m_eContext == S_BOOL) {
		sab = PutVectorANDGate(s, ab);
	} else {
		uint32_t svec = PutRepeaterGate(s, m_pGates[ab].nvals);
		sab = PutANDGate(svec, ab);
	}
	return PutXORGate(b, sab);
}




uint32_t BooleanCircuit::PutWideGate(e_gatetype type, vector<uint32_t> ins) {
	// build a balanced binary tree
	vector<uint32_t>& survivors = ins;

	while (survivors.size() > 1) {
		unsigned j = 0;
		for (unsigned i = 0; i < survivors.size();) {
			if (i + 1 >= survivors.size()) {
				survivors[j++] = survivors[i++];
			} else {
				if (type == G_NON_LIN)
					survivors[j++] = PutANDGate(survivors[i], survivors[i + 1]);
				else
					survivors[j++] = PutXORGate(survivors[i], survivors[i + 1]);

				i += 2;
			}
		}
		survivors.resize(j);
	}
	return survivors[0];
}


//compute the AND over all inputs
uint32_t BooleanCircuit::PutLUTWideANDGate(vector<uint32_t> ins) {
	// build a balanced 8-wise tree
	vector<uint32_t>& survivors = ins;
	uint64_t* lut = (uint64_t*) calloc(4, sizeof(uint64_t));
	uint32_t nins, maxins = 7, minins = 3;
	vector<uint32_t> lut_ins;
	uint32_t table_bitlen = sizeof(uint64_t) * 8;
	/*cout << "Building a balanced tree" << endl;
	cout << "Input gates: ";
	for(uint32_t i = 0; i < ins.size(); i++) {
		cout << ins[i] << ", ";
	}
	cout << endl;*/

	while (survivors.size() > 1) {
		uint32_t j = 0;

		for (uint32_t i = 0; i < survivors.size();) {
			nins = min((uint32_t) survivors.size()-i, maxins);
			if(nins <= minins && survivors.size() > minins) {
				for(; i < survivors.size();) {
					survivors[j++] = survivors[i++];
				}
			} else {
				lut_ins.clear();
				lut_ins.assign(ins.begin() + i, ins.begin() + i + nins);
				/*cout << "Combining " << nins << " gates: ";
				for(uint32_t k = 0; k < lut_ins.size(); k++) {
					cout << lut_ins[k] << ", ";
				}*/
				memset(lut, 0, bits_in_bytes(table_bitlen));
				lut[((1L<<nins)-1) / table_bitlen] = 1L << (((1L<<nins)-1) % table_bitlen);

				survivors[j++] = PutTruthTableGate(lut_ins, 1, lut);
				//cout << " to gate " << survivors[j-1] << endl;

				/*cout << "LUT: ";
				for(uint32_t k = 0; k < 4; k++) {
					cout << (hex) << lut[k] << ", " << (dec);
				}
				cout << endl;*/

				i += nins;
			}

		}
		survivors.resize(j);
	}
	return survivors[0];
}

//if s == 0: a stays a, else a becomes b, share interface
share** BooleanCircuit::PutCondSwapGate(share* a, share* b, share* s, BOOL vectorized) {
	share** s_out = (share**) malloc(sizeof(share*) *2);

	vector<vector<uint32_t> > out = PutCondSwapGate(a->get_wires(), b->get_wires(), s->get_wire_id(0), vectorized);
	s_out[0] = new boolshare(out[0], this);
	s_out[1] = new boolshare(out[1], this);
	return s_out;
}


//if s == 0: a stays a, else a becomes b
vector<vector<uint32_t> > BooleanCircuit::PutCondSwapGate(vector<uint32_t> a, vector<uint32_t> b, uint32_t s, BOOL vectorized) {
	uint32_t rep = min(a.size(), b.size());
	PadWithLeadingZeros(a, b);

	vector<vector<uint32_t> > out(2);
	out[0].resize(rep);
	out[1].resize(rep);

	//cout << "b.nvals = " << m_pGates[b[0]].nvals << ", b.size = " << b.size() <<  endl;

	uint32_t ab, snab, svec;

	if (m_eContext == S_BOOL) {
		if (vectorized) {
			out[0].resize(1);
			out[1].resize(1);

			ab = PutXORGate(a[0], b[0]);
			snab = PutVectorANDGate(s, ab);
			//uint32_t svec = PutRepeaterGate(s, 32);
			//snab = PutANDGate(svec, ab);
			out[0][0] = PutXORGate(snab, a[0]);
			out[1][0] = PutXORGate(snab, b[0]);
		} else {
			//Put combiner and splitter gates
			uint32_t avec = PutCombinerGate(a);
			uint32_t bvec = PutCombinerGate(b);

			ab = PutXORGate(avec, bvec);
			snab = PutVectorANDGate(s, ab);
			out[0] = PutSplitterGate(PutXORGate(snab, avec));
			out[1] = PutSplitterGate(PutXORGate(snab, bvec));
		}

	} else {
		if (m_pGates[s].nvals < m_pGates[a[0]].nvals)
				svec = PutRepeaterGate(s, m_pGates[a[0]].nvals);
			else
				svec = s;
			//cout << "b.nvals = " << m_pGates[b[0]].nvals << ", b.size = " << b.size() <<  endl;

			for (uint32_t i = 0; i < rep; i++) {
				ab = PutXORGate(a[i], b[i]);
				//snab = PutVectorANDGate(ab, s);

				snab = PutANDGate(svec, ab);

				//swap here to change swap-behavior of condswap
				out[0][i] = PutXORGate(snab, a[i]);
				out[1][i] = PutXORGate(snab, b[i]);
			}
	}

	/*uint32_t avec = m_cCircuit->PutCombinerGate(a);
	 uint32_t bvec = m_cCircuit->PutCombinerGate(b);

	 uint32_t abvec = PutXORGate(avec, bvec);
	 uint32_t snabvec = PutVectorANDGate(s, abvec);

	 out[0] = m_cCircuit->PutSplitterGate(PutXORGate(snabvec, avec));
	 out[1] = m_cCircuit->PutSplitterGate(PutXORGate(snabvec, bvec));*/

	/*for(uint32_t i=0; i<rep; i++)
	 {
	 ab = PutXORGate(a[i], b[i]);
	 //snab = PutVectorANDGate(ab, s);
	 snab = PutANDGate(s, ab);

	 //swap here to change swap-behavior of condswap
	 out[0][i] = PutXORGate(snab, a[i]);
	 out[1][i] = PutXORGate(snab, b[i]);
	 }*/
	return out;
}

//Returns val if b==1 and 0 else
vector<uint32_t> BooleanCircuit::PutELM0Gate(vector<uint32_t> val, uint32_t b) {
	vector<uint32_t> out(val.size());
	for (uint32_t i = 0; i < val.size(); i++) {
		out[i] = PutANDGate(val[i], b);
	}
	return out;
}

share* BooleanCircuit::PutMinGate(share** a, uint32_t nvals) {
	vector<vector<uint32_t> > min(nvals);
	uint32_t i;
	for (i = 0; i < nvals; i++) {
		min[i] = a[i]->get_wires();
	}
	return new boolshare(PutMinGate(min), this);
}


vector<uint32_t> BooleanCircuit::PutMinGate(vector<vector<uint32_t> > a) {
	// build a balanced binary tree
	uint32_t cmp;
	uint32_t avec, bvec;
	vector<vector<uint32_t> > m_vELMs = a;

	while (m_vELMs.size() > 1) {
		unsigned j = 0;
		for (unsigned i = 0; i < m_vELMs.size();) {
			if (i + 1 >= m_vELMs.size()) {
				m_vELMs[j] = m_vELMs[i];
				i++;
				j++;
			} else {
				//	cmp = bc->PutGTTree(m_vELMs[i], m_vELMs[i+1]);
				if (m_eContext == S_YAO) {
					cmp = PutSizeOptimizedGTGate(m_vELMs[i], m_vELMs[i + 1]);
					m_vELMs[j] = PutMUXGate(m_vELMs[i + 1], m_vELMs[i], cmp);
				} else {
					cmp = PutDepthOptimizedGTGate(m_vELMs[i], m_vELMs[i + 1]);
					m_vELMs[j] = PutMUXGate(m_vELMs[i + 1], m_vELMs[i], cmp);
				}

				i += 2;
				j++;
			}
		}
		m_vELMs.resize(j);
	}
	return m_vELMs[0];
}



// vals = values, ids = indicies of each value, n = size of vals and ids
void BooleanCircuit::PutMinIdxGate(share** vals, share** ids, uint32_t nvals, share** minval_shr, share** minid_shr) {
	vector<vector<uint32_t> > val(nvals);
	vector<vector<uint32_t> > id(nvals);

	vector<uint32_t> minval(1);
	vector<uint32_t> minid(1);

	for (uint32_t i = 0; i < nvals; i++) {
		/*val[i].resize(a[i]->size());
		for(uint32_t j = 0; j < a[i]->size(); j++) {
			val[i][j] = a[i]->get_wire(j);//->get_wires();
		}

		ids[i].resize(b[i]->size());
		for(uint32_t j = 0; j < b[i]->size(); j++) {
			ids[i][j] = b[i]->get_wire(j);
		}*/
		val[i] = vals[i]->get_wires();
		id[i] = ids[i]->get_wires();
	}

	PutMinIdxGate(val, id, minval, minid);

	*minval_shr = new boolshare(minval, this);
	*minid_shr = new boolshare(minid, this);
}


// vals = values, ids = indicies of each value, n = size of vals and ids
void BooleanCircuit::PutMinIdxGate(vector<vector<uint32_t> > vals, vector<vector<uint32_t> > ids,
		vector<uint32_t>& minval, vector<uint32_t>& minid) {
	// build a balanced binary tree
	uint32_t cmp;
	uint32_t avec, bvec;
	vector<vector<uint32_t> > m_vELMs = vals;

#ifdef USE_MULTI_MUX_GATES
	uint32_t nvariables = 2;
	share **vala, **valb, **valout, *tmpval, *tmpidx, *cond;
	if(m_eContext == S_BOOL) {
		vala = (share**) malloc(sizeof(share*) * nvariables);
		valb = (share**) malloc(sizeof(share*) * nvariables);
		valout = (share**) malloc(sizeof(share*) * nvariables);
		tmpval = new boolshare(vals[0].size(), this);
		tmpidx = new boolshare(ids[0].size(), this);
		cond = new boolshare(1, this);
	}
#endif

	while (m_vELMs.size() > 1) {
		unsigned j = 0;
		for (unsigned i = 0; i < m_vELMs.size();) {
			if (i + 1 >= m_vELMs.size()) {
				m_vELMs[j] = m_vELMs[i];
				ids[j] = ids[i];
				i++;
				j++;
			} else {
				//	cmp = bc->PutGTTree(m_vELMs[i], m_vELMs[i+1]);
				if (m_eContext == S_BOOL) {
					cmp = PutDepthOptimizedGTGate(m_vELMs[i], m_vELMs[i + 1]);
#ifdef USE_MULTI_MUX_GATES
					//Multimux
					cond->set_wire_id(0, cmp);
					vala[0] = new boolshare(m_vELMs[i+1], this);
					vala[1] = new boolshare(ids[i+1], this);

					valb[0] = new boolshare(m_vELMs[i], this);
					valb[1] = new boolshare(ids[i], this);

					valout[0] = tmpval;
					valout[1] = tmpidx;

					PutMultiMUXGate(vala, valb, cond, nvariables, valout);
					m_vELMs[j] = tmpval->get_wires();
					ids[j] = tmpidx->get_wires();
#else
					m_vELMs[j] = PutMUXGate(m_vELMs[i + 1], m_vELMs[i], cmp, false);
					ids[j] = PutMUXGate(ids[i + 1], ids[i], cmp, false);
#endif
				} else {
					cmp = PutSizeOptimizedGTGate(m_vELMs[i], m_vELMs[i + 1]);
					m_vELMs[j] = PutMUXGate(m_vELMs[i + 1], m_vELMs[i], cmp);
					ids[j] = PutMUXGate(ids[i + 1], ids[i], cmp);

				}

				i += 2;
				j++;
			}
		}
		m_vELMs.resize(j);
		ids.resize(j);
	}
	minval = m_vELMs[0];
	minid = ids[0];

#ifdef USE_MULTI_MUX_GATES
	if(m_eContext == S_BOOL) {
		free(vala);
		free(valb);
		free(valout);
		delete tmpval;
		delete tmpidx;
		delete cond;
	}
#endif
}

/**Max....*/

// vals = values, ids = indicies of each value, n = size of vals and ids
void BooleanCircuit::PutMaxIdxGate(share** vals, share** ids, uint32_t nvals, share** maxval_shr, share** maxid_shr) {
	vector<vector<uint32_t> > val(nvals);
	vector<vector<uint32_t> > id(nvals);

	vector<uint32_t> maxval(1);
	vector<uint32_t> maxid(1);

	for (uint32_t i = 0; i < nvals; i++) {
		/*val[i].resize(a[i]->size());
		for(uint32_t j = 0; j < a[i]->size(); j++) {
			val[i][j] = a[i]->get_wire(j);//->get_wires();
		}

		ids[i].resize(b[i]->size());
		for(uint32_t j = 0; j < b[i]->size(); j++) {
			ids[i][j] = b[i]->get_wire(j);
		}*/
		val[i] = vals[i]->get_wires();
		id[i] = ids[i]->get_wires();
	}

	//cout<<"Size: "<<val.size()<<endl;
	PutMaxIdxGate(val, id, maxval, maxid);

	*maxval_shr = new boolshare(maxval, this);
	*maxid_shr = new boolshare(maxid, this);
}


// vals = values, ids = indicies of each value, n = size of vals and ids
void BooleanCircuit::PutMaxIdxGate(vector<vector<uint32_t> > vals, vector<vector<uint32_t> > ids,
		vector<uint32_t>& maxval, vector<uint32_t>& maxid) {
	// build a balanced binary tree
	uint32_t cmp;
	uint32_t avec, bvec;
	vector<vector<uint32_t> > m_vELMs = vals;
#ifdef USE_MULTI_MUX_GATES
	uint32_t nvariables = 2;
	share **vala, **valb, **valout, *tmpval, *tmpidx, *cond;
	if(m_eContext == S_BOOL) {
		vala = (share**) malloc(sizeof(share*) * nvariables);
		valb = (share**) malloc(sizeof(share*) * nvariables);
		valout = (share**) malloc(sizeof(share*) * nvariables);
		tmpval = new boolshare(vals[0].size(), this);
		tmpidx = new boolshare(ids[0].size(), this);
		cond = new boolshare(1, this);
	}
#endif

	while (m_vELMs.size() > 1) {
		unsigned j = 0;
		for (unsigned i = 0; i < m_vELMs.size();) {
			if (i + 1 >= m_vELMs.size()) {
				m_vELMs[j] = m_vELMs[i];
				ids[j] = ids[i];
				i++;
				j++;
			} else {
				if (m_eContext == S_BOOL) {
					cmp = PutDepthOptimizedGTGate(m_vELMs[i+1], m_vELMs[i]);

#ifdef USE_MULTI_MUX_GATES
					//Multimux
					cond->set_wire_id(0, cmp);
					vala[0] = new boolshare(m_vELMs[i+1], this);
					vala[1] = new boolshare(ids[i+1], this);

					valb[0] = new boolshare(m_vELMs[i], this);
					valb[1] = new boolshare(ids[i], this);

					valout[0] = tmpval;
					valout[1] = tmpidx;

					PutMultiMUXGate(vala, valb, cond, nvariables, valout);
					m_vELMs[j] = tmpval->get_wires();
					ids[j] = tmpidx->get_wires();
#else
					m_vELMs[j] = PutMUXGate(m_vELMs[i + 1], m_vELMs[i], cmp);
					ids[j] = PutMUXGate(ids[i + 1], ids[i], cmp);
#endif
				} else {
					cmp = PutSizeOptimizedGTGate(m_vELMs[i + 1], m_vELMs[i]);
					m_vELMs[j] = PutMUXGate(m_vELMs[i + 1], m_vELMs[i], cmp);
					ids[j] = PutMUXGate(ids[i + 1], ids[i], cmp);

				}

				i += 2;
				j++;
			}
		}
		m_vELMs.resize(j);
		ids.resize(j);
	}
	maxval = m_vELMs[0];
	maxid = ids[0];

#ifdef USE_MULTI_MUX_GATES
	if(m_eContext == S_BOOL) {
		free(vala);
		free(valb);
		free(valout);
		delete tmpval;
		delete tmpidx;
		delete cond;
	}
#endif
}




vector<uint32_t> BooleanCircuit::PutFPGate(const string func, vector<uint32_t> inputs, uint8_t bitsize, uint32_t nvals){
	string fn = "circ/fp_";
	char bs[3];
	fn += func;
	fn += "_";
	cout << "bs = " << (uint32_t) bitsize << endl;
	sprintf(bs, "%d", bitsize);
	fn += bs;
	fn += ".aby";
	cout << "opening " << fn.c_str() << endl;
	return PutGateFromFile(fn.c_str(), inputs, nvals);
}


vector<uint32_t> BooleanCircuit::PutFPGate(const string func, vector<uint32_t> ina, vector<uint32_t> inb, uint8_t bitsize, uint32_t nvals){
	ina.insert(ina.end(), inb.begin(), inb.end());
	return PutFPGate(func, ina, bitsize, nvals);
}


vector<uint32_t> BooleanCircuit::PutGateFromFile(const string filename, vector<uint32_t> inputs, uint32_t nvals){
	string line;
	vector<uint32_t> tokens, outputs;
	map<uint32_t, uint32_t> wires;

	ifstream myfile;

	//cout << "opening " << filename <<  endl;
	myfile.open(filename.c_str());

	uint32_t file_input_size = 0;

	if (myfile.is_open()) {
		while (getline(myfile, line)) {

			if (line != "") {

				tokenize_verilog(line, tokens);

				switch (line.at(0)) {

				case 'S': // Server input wire ids
					assert(inputs.size() >= tokens.size() + file_input_size);

					for (uint32_t i = 0; i < tokens.size(); i++) {
						wires[tokens[i]] = inputs[i + file_input_size];
					}
					file_input_size += tokens.size();
					break;

				case 'C': // Client input wire ids
					assert(inputs.size() >= tokens.size() + file_input_size);

					for (uint32_t i = 0; i < tokens.size(); i++) {
						wires[tokens[i]] = inputs[i + file_input_size];
					}
					file_input_size += tokens.size();
					break;

				case '0': // Constant Zero Gate
					wires[tokens[0]] = PutConstantGate(0, nvals);
					break;

				case '1': // Constant Zero Gate
					wires[tokens[0]] = PutConstantGate((1 << nvals) - 1, nvals);
					break;

				case 'A': // AND Gate
					wires[tokens[2]] = PutANDGate(wires[tokens[0]], wires[tokens[1]]);
					break;

				case 'X': // XOR Gate
					wires[tokens[2]] = PutXORGate(wires[tokens[0]], wires[tokens[1]]);
					break;

				case 'M': // MUX Gate
					wires[tokens[3]] = PutVecANDMUXGate(wires[tokens[1]], wires[tokens[0]], wires[tokens[2]]);
					break;

				case 'I': // INV Gate
					wires[tokens[1]] = PutINVGate(tokens[0]);
					break;

				case 'O': // List of output wires
					for (uint32_t i = 0; i < tokens.size(); i++) {
						outputs.push_back(wires[tokens[i]]);
					}
					cout << endl;
					break;
				}
			}
		}
		myfile.close();

		if (file_input_size < inputs.size()) {
			cerr << "Warning: Input sizes didn't match! Less inputs read from circuit file than passed to it!" << endl;
		}
	}

	else {
		cerr << "Error: Unable to open circuit file " << filename << endl;
	}

	wires.clear();
	tokens.clear();

	return outputs;
}

uint32_t BooleanCircuit::GetInputLengthFromFile(const string filename){
	string line;
	vector<uint32_t> tokens;
	ifstream myfile;

	//cout << "opening " << filename <<  endl;
	myfile.open(filename.c_str());

	uint32_t file_input_size = 0;

	if (myfile.is_open()) {
		while (getline(myfile, line)) {

			if (line != "") {

				tokenize_verilog(line, tokens);

				switch (line.at(0)) {

				case 'S': // Server input wire ids
					file_input_size += tokens.size();
					break;

				case 'C': // Client input wire ids
					file_input_size += tokens.size();
					break;
				}
			}
		}
		myfile.close();
	}

	else {
		cerr << "Error: Unable to open circuit file " << filename << endl;
	}

	tokens.clear();

	return file_input_size;
}

uint32_t BooleanCircuit::PutIdxGate(uint32_t r, uint32_t maxidx) {
	if (r > maxidx) {
		r = maxidx;
		cout << "Warning: Index bigger than maxidx for IndexGate" << endl;
	}
	uint32_t digit, limit = ceil_log2(maxidx);
	vector<uint32_t> temp(limit);	// = m_nFrontier;
#ifdef ZDEBUG
			cout << "index for r = " << r << endl;
#endif
	for (uint32_t j = 0; j < limit; j++) {
		digit = (r >> j) & 1;

		temp[j] = PutConstantGate((UGATE_T) digit, 1);
		//cout << "gate: " << out[j] << ": " << digit << endl;
	}

	return PutCombinerGate(temp);
}

void BooleanCircuit::PutMultiMUXGate(share** Sa, share** Sb, share* sel, uint32_t nshares, share** Sout) {

	vector<uint32_t> inputsa, inputsb;
	uint32_t *posids;
	uint32_t bitlen = 0;
	uint32_t nvals = m_pGates[sel->get_wire_id(0)].nvals;

	//Yao not allowed, if so just put standard muxes
	assert(m_eContext == S_BOOL);

	for(uint32_t i = 0; i < nshares; i++) {
		bitlen += Sa[i]->get_bitlength();
	}
	uint32_t total_nvals = bitlen * nvals;
	share* vala = new boolshare(bitlen, this);
	share* valb = new boolshare(bitlen, this);

	//cout << "setting gate" << endl;
	for(uint32_t i = 0, idx; i < bitlen; i++) {
		for(uint32_t j = 0, ctr = 0; j < nshares && (i >= ctr || j == 0); j++) {
			if(i < (ctr+Sa[j]->get_bitlength())) {
				idx = i - ctr;
				//cout << "for i = " << i << " taking j = " << j << " and ctr = " << ctr << endl;
				vala->set_wire_id(i, Sa[j]->get_wire_id(idx));
				valb->set_wire_id(i, Sb[j]->get_wire_id(idx));
			}
			ctr+=Sa[j]->get_bitlength();
		}
	}

	share* avec = PutStructurizedCombinerGate(vala, 0, 1, total_nvals);
	share* bvec = PutStructurizedCombinerGate(valb, 0, 1, total_nvals);

	share* out = PutVecANDMUXGate(avec, bvec, sel);

	//cout << "Setting out gates "  << endl;
	for(uint32_t i = 0, idx; i < bitlen; i++) {
		for(uint32_t j = 0, ctr = 0; j < nshares && (i >= ctr || j == 0); j++) {
			if(i < (ctr+Sa[j]->get_bitlength())) {
				idx = i - ctr;
				Sout[j]->set_wire_id(idx, PutStructurizedCombinerGate(out, i, bitlen, nvals)->get_wire_id(0));
			}
			ctr+=Sa[j]->get_bitlength();
		}

	}

}

void BooleanCircuit::Reset() {
	Circuit::Reset();

	free(m_vANDs);
	m_nNumANDSizes = 1;
	m_vANDs = (non_lin_vec_ctx*) malloc(sizeof(non_lin_vec_ctx) * m_nNumANDSizes);
	m_vANDs[0].bitlen = 1;
	m_vANDs[0].numgates = 0;
	m_nB2YGates = 0;
	m_nA2YGates = 0;
	m_nYSwitchGates = 0;
	m_nNumXORVals = 0;
	m_nNumXORGates = 0;

	m_vTTlens.resize(1);
	m_vTTlens[0].resize(1);
	m_vTTlens[0][0].resize(1);
	m_vTTlens[0][0][0].tt_len = 4;
	m_vTTlens[0][0][0].numgates = 0;
	m_vTTlens[0][0][0].out_bits = 1;

	/*free(m_vTTlens);
	m_nNumTTSizes = 1;
	m_vTTlens = (tt_lens_ctx*) malloc(sizeof(tt_lens_ctx) * m_nNumTTSizes);
	m_vTTlens[0].tt_len = 4;
	m_vTTlens[0].numgates = 0;*/
}

void BooleanCircuit::PadWithLeadingZeros(vector<uint32_t> &a, vector<uint32_t> &b) {
	uint32_t maxlen = max(a.size(), b.size());
	if(a.size() != b.size()) {
		uint32_t zerogate = PutConstantGate(0, m_pGates[a[0]].nvals);
		a.resize(maxlen, zerogate);
		b.resize(maxlen, zerogate);
	}
}

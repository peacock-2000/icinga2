/* Icinga 2 | (c) 2023 Icinga GmbH | GPLv2+ */

#include "remote/filterutility.hpp"

using namespace icinga;

/**
 * If the given assign filter is like the following, extract the host+service names ("H"+"S", "h"+"s", ...) into the vector:
 *
 * host.name == "H" && service.name == "S" [ || host.name == "h" && service.name == "s" ... ]
 *
 * The order of operands of || && == doesn't matter.
 *
 * @returns Whether the given assign filter is like above.
 */
bool FilterUtility::GetTargetServices(Expression* filter, const Dictionary::Ptr& constants, std::vector<std::pair<const String *, const String *>>& out)
{
	auto lor (dynamic_cast<LogicalOrExpression*>(filter));

	if (lor) {
		return GetTargetServices(lor->GetOperand1().get(), constants, out)
			&& GetTargetServices(lor->GetOperand2().get(), constants, out);
	}

	auto service (GetTargetService(filter, constants));

	if (service.first) {
		out.emplace_back(service);
		return true;
	}

	return false;
}

/**
 * If the given filter is like the following, extract the host+service names ("H"+"S"):
 *
 * host.name == "H" && service.name == "S"
 *
 * The order of operands of && == doesn't matter.
 *
 * @returns {host, service} on success, {nullptr, nullptr} on failure.
 */
std::pair<const String *, const String *> FilterUtility::GetTargetService(Expression* filter, const Dictionary::Ptr& constants)
{
	auto land (dynamic_cast<LogicalAndExpression*>(filter));

	if (!land) {
		return {nullptr, nullptr};
	}

	auto op1 (land->GetOperand1().get());
	auto op2 (land->GetOperand2().get());
	auto host (GetComparedName(op1, "host", constants));

	if (!host) {
		std::swap(op1, op2);
		host = GetComparedName(op1, "host", constants);
	}

	if (host) {
		auto service (GetComparedName(op2, "service", constants));

		if (service) {
			return {host, service};
		}
	}

	return {nullptr, nullptr};
}

/**
 * If the given assign filter is like the following, extract the host names ("H", "h", ...) into the vector:
 *
 * host.name == "H" [ || host.name == "h" ... ]
 *
 * The order of operands of || == doesn't matter.
 *
 * @returns Whether the given assign filter is like above.
 */
bool FilterUtility::GetTargetHosts(Expression* filter, const Dictionary::Ptr& constants, std::vector<const String *>& out)
{
	auto lor (dynamic_cast<LogicalOrExpression*>(filter));

	if (lor) {
		return GetTargetHosts(lor->GetOperand1().get(), constants, out)
			&& GetTargetHosts(lor->GetOperand2().get(), constants, out);
	}

	auto name (GetComparedName(filter, "host", constants));

	if (name) {
		out.emplace_back(name);
		return true;
	}

	return false;
}

/**
 * If the given filter is like the following, extract the object name ("N"):
 *
 * $lcType$.name == "N"
 *
 * The order of operands of == doesn't matter.
 *
 * @returns The object name on success, nullptr on failure.
 */
const String * FilterUtility::GetComparedName(Expression* filter, const char * lcType, const Dictionary::Ptr& constants)
{
	auto eq (dynamic_cast<EqualExpression*>(filter));

	if (!eq) {
		return nullptr;
	}

	auto op1 (eq->GetOperand1().get());
	auto op2 (eq->GetOperand2().get());

	if (IsNameIndexer(op1, lcType, constants)) {
		return GetConstString(op2, constants);
	}

	if (IsNameIndexer(op2, lcType, constants)) {
		return GetConstString(op1, constants);
	}

	return nullptr;
}

/**
 * @returns Whether the given expression is like $lcType$.name.
 */
bool FilterUtility::IsNameIndexer(Expression* exp, const char * lcType, const Dictionary::Ptr& constants)
{
	auto ixr (dynamic_cast<IndexerExpression*>(exp));

	if (!ixr) {
		return false;
	}

	auto var (dynamic_cast<VariableExpression*>(ixr->GetOperand1().get()));

	if (!var || var->GetVariable() != lcType) {
		return false;
	}

	auto val (GetConstString(ixr->GetOperand2().get(), constants));

	return val && *val == "name";
}

/**
 * @returns If the given expression is a constant string, its address. nullptr on failure.
 */
const String * FilterUtility::GetConstString(Expression* exp, const Dictionary::Ptr& constants)
{
	auto cnst (GetConst(exp, constants));

	return cnst && cnst->IsString() ? &cnst->Get<String>() : nullptr;
}

/**
 * @returns If the given expression is a constant, its address. nullptr on failure.
 */
const Value * FilterUtility::GetConst(Expression* exp, const Dictionary::Ptr& constants)
{
	auto lit (dynamic_cast<LiteralExpression*>(exp));

	if (lit) {
		return &lit->GetValue();
	}

	if (constants) {
		auto var (dynamic_cast<VariableExpression*>(exp));

		if (var) {
			return constants->GetRef(var->GetVariable());
		}
	}

	return nullptr;
}

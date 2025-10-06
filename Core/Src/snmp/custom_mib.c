#include "lwip/apps/snmp_scalar.h"
#include "snmp/custom_mib.h"
#include "signal_processor.h"
/* Базовый OID: 1.3.6.1.4.1.55555.1 */
static const u32_t custom_base_oid[] = { 1,3,6,1,4,1,62283,1 };



/* Колбэки для чтения переменных */
static s16_t custom_get_value(struct snmp_node_instance* instance, void* value) {
  *(u32_t*)value = voltage1;
  return sizeof(u32_t);
}

static s16_t custom_get_var1(struct snmp_node_instance* instance, void* value) {
  *(u32_t*)value = voltage2;
  return sizeof(u32_t);
}

static s16_t custom_get_var2(struct snmp_node_instance* instance, void* value) {
  *(u32_t*)value = current;
  return sizeof(u32_t);
}

static s16_t custom_get_var3(struct snmp_node_instance* instance, void* value) {
  *(u32_t*)value = 123;
  return sizeof(u32_t);
}

static s16_t custom_get_var4(struct snmp_node_instance* instance, void* value) {
  *(u32_t*)value = 228;
  return sizeof(u32_t);
}

/* Узлы для каждой переменной */
static const struct snmp_scalar_node custom_mib_node =
  SNMP_SCALAR_CREATE_NODE_READONLY(1, SNMP_ASN1_TYPE_INTEGER, custom_get_value);

static const struct snmp_scalar_node custom_mib_node1 =
  SNMP_SCALAR_CREATE_NODE_READONLY(2, SNMP_ASN1_TYPE_INTEGER, custom_get_var1);

static const struct snmp_scalar_node custom_mib_node2 =
  SNMP_SCALAR_CREATE_NODE_READONLY(3, SNMP_ASN1_TYPE_INTEGER, custom_get_var2);

static const struct snmp_scalar_node custom_mib_node3 =
  SNMP_SCALAR_CREATE_NODE_READONLY(4, SNMP_ASN1_TYPE_INTEGER, custom_get_var3);

static const struct snmp_scalar_node custom_mib_node4 =
  SNMP_SCALAR_CREATE_NODE_READONLY(5, SNMP_ASN1_TYPE_INTEGER, custom_get_var4);

/* Корень (дерево) со всеми узлами */
static const struct snmp_node* const custom_nodes[] = {
  &custom_mib_node.node,
  &custom_mib_node1.node,
  &custom_mib_node2.node,
  &custom_mib_node3.node,
  &custom_mib_node4.node
};

static const struct snmp_tree_node custom_mib_root =
  SNMP_CREATE_TREE_NODE(1, custom_nodes);

/* Сам MIB */
const struct snmp_mib custom_mib =
  SNMP_MIB_CREATE(custom_base_oid, &custom_mib_root.node);

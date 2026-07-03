//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/value/value.h"

typedef struct ten_schema_t ten_schema_t;

/**
 * @param schemas_content The schema definition contains the 'property' part.
 * Ex:
 * 1) The definition of the property of extension is as follows:
 *
 *		{
 *				"property": {
 *					"foo": {
 *						"type": "string"
 *					}
 *				},
 *				"cmd_in": []
 *		}
 *
 * 2) The definition of the property of the msg is as follows:
 *
 *		{
 *				"cmd_in": [
 *					{
 *						"name": "demo",
 *						"property": {
 *							"foo": {
 *								"type": "string"
 *							}
 *						},
 *						"required": ["foo"]
 *					}
 *				]
 *		}
 *
 * The type of the property schema is always `ten_schema_object_t` in the
 * schema system. We prefer to parse the `property` schema into a
 * `ten_schema_object_t`, but not a hashmap<name, ten_schema_t>, because some
 * schema definition which is at the same level as `property` (ex: `required`)
 * is used to describe the property.
 */
TEN_RUNTIME_PRIVATE_API ten_schema_t *
ten_schemas_parse_schema_object_for_property(ten_value_t *schemas_content);

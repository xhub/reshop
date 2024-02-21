/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2018 INRIA.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/** @file level_logger.h
 *
 *  @brief Common data structures used by loggers
 *
 */

#ifndef RHP_LOGGER_H
#define RHP_LOGGER_H

/** loglevels for the loggers in numerics*/
typedef enum {
  RHP_LOGLEVEL_NO,
  RHP_LOGLEVEL_BASIC,
  RHP_LOGLEVEL_LIGHT,
  RHP_LOGLEVEL_VEC,
  RHP_LOGLEVEL_MAT,
  RHP_LOGLEVEL_ALL
} RHP_loglevels;

#define RHP_LOG_LIGHT(log_lvl, expr) if (log_lvl >= RHP_LOGLEVEL_LIGHT) expr;
#define RHP_LOG_SCALAR(log_lvl, expr) RHP_LOG_LIGHT(log_lvl, expr)
#define RHP_LOG_VEC(log_lvl, expr) if (log_lvl >= RHP_LOGLEVEL_VEC) expr;
#define RHP_LOG_MAT(log_lvl, expr) if (log_lvl >= RHP_LOGLEVEL_MAT) expr;

#endif

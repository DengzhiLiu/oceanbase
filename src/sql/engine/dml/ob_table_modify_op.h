/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OCEANBASE_SQL_ENGINE_DML_OB_TABLE_MODIFY_OP_
#define OCEANBASE_SQL_ENGINE_DML_OB_TABLE_MODIFY_OP_

#include "sql/engine/ob_operator.h"
#include "sql/engine/dml/ob_dml_ctx_define.h"
#include "observer/ob_inner_sql_connection.h"
#include "sql/engine/ob_exec_context.h"

namespace oceanbase
{
namespace sql
{
class ForeignKeyHandle
{
public:
  struct ObFkRowResInfo
  {
    ObExpr* rt_expr_;
    ObDatum ori_datum_;
    TO_STRING_KV(K_(rt_expr), K_(ori_datum));
  };
  static int do_handle(ObTableModifyOp &op,
                       const ObDMLBaseCtDef &dml_ctdef,
                       ObDMLBaseRtDef &dml_rtdef);
private:
  static int value_changed(ObTableModifyOp &op,
                           const common::ObIArray<ObForeignKeyColumn> &columns,
                           const ObExprPtrIArray &old_row,
                           const ObExprPtrIArray &new_row,
                           bool &has_changed);
  static int check_exist(ObTableModifyOp &modify_op, const ObForeignKeyArg &fk_arg,
                         const ObExprPtrIArray &row, bool expect_zero);
  static int cascade(ObTableModifyOp &modify_op, const ObForeignKeyArg &fk_arg,
                     const ObExprPtrIArray &old_row, const ObExprPtrIArray &new_row);
  static int gen_set(ObEvalCtx &eval_ctx, char *&buf, int64_t &len, int64_t &pos,
                     const common::ObIArray<ObForeignKeyColumn> &columns,
                     const ObExprPtrIArray &row, common::ObIAllocator &alloc,
                     const common::ObObjPrintParams &print_params);
  static int gen_where(ObEvalCtx &eval_ctx, char *&buf, int64_t &len, int64_t &pos,
                       const common::ObIArray<ObForeignKeyColumn> &columns,
                       const ObExprPtrIArray &row, common::ObIAllocator &alloc,
                       const common::ObObjPrintParams &print_params);
  static int gen_column_value(ObEvalCtx &ctx, char *&buf, int64_t &len, int64_t &pos,
                              const common::ObIArray<ObForeignKeyColumn> &columns,
                              const ObExprPtrIArray &row, const char *delimiter,
                              common::ObIAllocator &alloc,
                              const common::ObObjPrintParams &print_params, bool forbid_null);
  static int is_self_ref_row(ObEvalCtx &ctx, const ObExprPtrIArray &row,
                             const ObForeignKeyArg &fk_arg, bool &is_self_ref);
};

class ObTableModifyOp;
class ObTableModifySpec : public ObOpSpec
{
  OB_UNIS_VERSION_V(1);
public:
  ObTableModifySpec(common::ObIAllocator &alloc, const ObPhyOperatorType type);
  virtual ~ObTableModifySpec() {}

  virtual bool is_dml_operator() const override { return true; }
  //This interface is only allowed to be used in a single-table DML operator,
  //it is invalid when multiple tables are modified in one DML operator
  int get_single_table_loc_id(common::ObTableID &table_loc_id,
                              common::ObTableID &ref_table_id) const
  {
    const ObDMLBaseCtDef *dml_ctdef = nullptr;
    int ret = get_single_dml_ctdef(dml_ctdef);
    if (common::OB_SUCCESS == ret) {
      table_loc_id = dml_ctdef->das_base_ctdef_.table_id_;
      ref_table_id = dml_ctdef->das_base_ctdef_.index_tid_;
    }
    return ret;
  }
  //This interface is only allowed to be used in a single-table DML operator,
  //it is invalid when multiple tables are modified in one DML operator
  virtual int get_single_dml_ctdef(const ObDMLBaseCtDef *&dml_ctdef) const
  {
    UNUSED(dml_ctdef);
    return common::OB_NOT_IMPLEMENT;
  }
  void set_table_location_uncertain(bool v) { table_location_uncertain_ = v; }
  bool is_table_location_uncertain() const { return table_location_uncertain_; }
  inline bool use_dist_das() const { return use_dist_das_; }
  inline bool has_instead_of_trigger() const { return has_instead_of_trigger_; }
public:
  // Expr frame info for partial expr serialization. (serialize is not need for it self)
  ObExprFrameInfo *expr_frame_info_;
  ObExpr *ab_stmt_id_; //mark the stmt id for array binding batch execution
  union {
    uint64_t flags_;
    struct {
      uint64_t is_ignore_                       : 1;
      uint64_t gi_above_                        : 1;
      uint64_t is_returning_                    : 1;
      uint64_t is_pdml_index_maintain_          : 1; // 表示当前dml算子是否是pdml中用于维护索引操作的算子（index maintain）
      uint64_t table_location_uncertain_        : 1; // 目标访问分区位置不确定，需要全表访问
      uint64_t use_dist_das_                    : 1;
      uint64_t has_instead_of_trigger_          : 1;
      uint64_t is_pdml_update_split_            : 1; // 标记delete, insert op是否由update拆分而来
      uint64_t reserved_                        : 56;
    };
  };
private:
  DISALLOW_COPY_AND_ASSIGN(ObTableModifySpec);
};

class ObTableModifyOpInput : public ObOpInput
{
public:
  friend class ObTableModifyOp;
  OB_UNIS_VERSION_V(1);
public:
  ObTableModifyOpInput(ObExecContext &ctx, const ObOpSpec &spec)
    : ObOpInput(ctx, spec),
      table_loc_(nullptr),
      tablet_loc_(nullptr)
  { }
  virtual ~ObTableModifyOpInput() { }
  virtual int init(ObTaskInfo &task_info) override { UNUSED(task_info); return common::OB_SUCCESS; }
  virtual void reset()
  {
    table_loc_ = nullptr;
    tablet_loc_ = nullptr;
  }
  inline void set_tablet_loc(ObDASTabletLoc *tablet_loc) { tablet_loc_ = tablet_loc; }
  inline ObDASTabletLoc *get_tablet_loc() { return tablet_loc_; }
  inline ObDASTableLoc *get_table_loc() { return table_loc_; }
  const ObTableModifySpec &get_spec() const
  {
    return static_cast<const ObTableModifySpec &>(spec_);
  }
  TO_STRING_KV(KPC_(table_loc), KPC_(tablet_loc));
protected:
  ObDASTableLoc *table_loc_;
  ObDASTabletLoc *tablet_loc_; //for single table modify op
private:
  DISALLOW_COPY_AND_ASSIGN(ObTableModifyOpInput);
};

class ObTableModifyOp: public ObOperator
{
public:
  ObTableModifyOp(ObExecContext &ctx, const ObOpSpec &spec, ObOpInput *input);
  virtual ~ObTableModifyOp() {}

  virtual int inner_switch_iterator() override;

  int is_valid();

  sql::ObSQLSessionInfo::StmtSavedValue &get_saved_session()
  {
    if (NULL == saved_session_) {
      saved_session_ = new (saved_session_buf_) sql::ObSQLSessionInfo::StmtSavedValue();
    }
    return *saved_session_;
  }

  const ObTableModifySpec &get_spec() const
  {
    return static_cast<const ObTableModifySpec &>(spec_);
  }
  ObTableModifyOpInput *get_input() const
  {
    return static_cast<ObTableModifyOpInput *>(input_);
  }

  virtual void destroy() override
  {
    dml_rtctx_.cleanup();
    trigger_clear_exprs_.reset();
    ObOperator::destroy();
  }

public:
  int open_inner_conn();
  int close_inner_conn();
  int begin_nested_session(bool skip_cur_stmt_tables);
  int end_nested_session();
  int set_foreign_key_cascade(bool is_cascade);
  int get_foreign_key_cascade(bool &is_cascade) const;
  int set_foreign_key_check_exist(bool is_check_exist);
  int get_foreign_key_check_exist(bool &is_check_exist) const;
  int execute_write(const char *sql);
  int execute_read(const char *sql, common::ObMySQLProxy::MySQLResult &res);
  int check_stack();
  bool is_nested_session() { return is_nested_session_; }
  void set_foreign_key_checks() { foreign_key_checks_ = true; }
  bool need_foreign_key_checks() { return foreign_key_checks_; }
  const ObObjPrintParams &get_obj_print_params() { return obj_print_params_; }
  int init_foreign_key_operation();
  void log_user_error_inner(int ret, int64_t col_idx, int64_t row_num,
                            const ObIArray<ColumnContent> &column_infos) const;
  void log_user_error_inner(int ret, int64_t row_num, const ColumnContent &column_info) const;
  void clear_dml_evaluated_flag();
  void clear_dml_evaluated_flag(int64_t parent_cnt, ObExpr **parent_exprs);
  void clear_dml_evaluated_flag(ObExpr *clear_expr);
protected:
  OperatorOpenOrder get_operator_open_order() const;
  virtual int inner_open();
  virtual int inner_close();

  int get_gi_task();
  //It is used for the execution without routing through DAS.
  //In this case, the DML operator only allows to modify a single table,
  //and the table location is specified by the scheduling framework.
  int calc_single_table_loc();

  virtual int inner_rescan() override;

  int submit_all_dml_task();
  int init_das_dml_ctx();
  //to merge array binding cusor info when array binding is executed in batch mode
  int merge_implict_cursor(int64_t insert_rows,
                           int64_t update_rows,
                           int64_t delete_rows,
                           int64_t found_rows);
public:
  common::ObMySQLProxy *sql_proxy_;
  observer::ObInnerSQLConnection *inner_conn_;
  uint64_t tenant_id_;
  observer::ObInnerSQLConnection::SavedValue saved_conn_;
  bool is_nested_session_;
  bool foreign_key_checks_;
  bool need_close_conn_;

  ObObjPrintParams obj_print_params_;
  bool iter_end_;
  ObDMLRtCtx dml_rtctx_;
  bool is_error_logging_;
  ObErrLogRtDef err_log_rt_def_;
  ObSEArray<ObExpr *, 4> trigger_clear_exprs_;
private:
  ObSQLSessionInfo::StmtSavedValue *saved_session_;
  char saved_session_buf_[sizeof(ObSQLSessionInfo::StmtSavedValue)] __attribute__((aligned (16)));;

  // used by check_rowkey_whether_distinct
  static const int64_t MIN_ROWKEY_DISTINCT_BUCKET_NUM = 1 * 1024;
  static const int64_t MAX_ROWKEY_DISTINCT_BUCKET_NUM = 1 * 1024 * 1024;
};

}  // namespace sql
}  // namespace oceanbase
#endif
